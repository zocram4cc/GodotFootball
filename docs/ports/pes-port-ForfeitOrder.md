# ForfeitOrder: does PES award the 3-0 forfeit before the level-score decision?

Question (Main, 12-11-26): `PesForfeitMatch.AYellowThenRedStillOnThePitchAtFullTimeForfeitsThreeNil` fails on `MoveStepCell` `aea2e6ce` and passes on `7954923d`. Does PES evaluate the forfeit before the level decision at the end of regulation, and does a forfeit stop extra time and penalties?

Setup: worktree `<workdrive>/gf-ForfeitOrder`, branch `ForfeitOrder` at `origin/main` `a4689f71` (no commits; `extern/godot-cpp` symlinked). No code change. No sim run (none needed for the asm answer). `GF_PES_SURVEY` empty at start.

## Answer

1. **No.** The red-state forfeit (writer 1, `FUN_140A23260`) is gated on `MI+0x1168 == 10` (full time). The level decision is taken at the regulation whistle by the period writer (`FUN_1404494E0` with `FUN_1404493D0` / `FUN_1404499D0`), which picks the next period. Writer 1 can only run after that choice.
2. **No.** No forfeit state is read by the tie tests, so a forfeit cannot change the level decision, extra time or the shoot-out. Extra time and the shoot-out are routed by two settings bytes of the match settings object `E = *(G+0x2C0)`: `E+0x66` (extra time) and `E+0x67` (shoot-out). The writers of those bytes are not located.
3. **Minimum players** (`FUN_140447B50` case 6) is a different rule. It writes period 10 with no score test, so when it runs it pre-empts the level decision. It is not in the regulation-end programme (see the chain), so PES does not evaluate it at the regulation whistle. It is in the break before the shoot-out (period 8), where it can stop the shoot-out.
4. **Consequence for the test.** In a non-knockout match (`E+0x66 = 0`, `E+0x67 = 0`) a level score ends at full time with no extra time. Writer 1 then applies the forfeit once its caller runs with the period at 10 (caveat in the chain). So "3-0 at full time, no extra time" is PES's result for that route. GF's stand-in is the knockout route (extra time, then shoot-out), so GF never reaches it.
5. **GF gap (independent of ordering).** In the knockout route PES reaches period 10 after the shoot-out (captures labelled 8-10; see below), where writer 1 would apply. GF cannot: `PesClock::SetPhase(Penalties)` sets period 9 (`pesclock.cpp:317`, `PeriodNumber` 9 at `:86-95`), so the shoot-out's `GameOver()` (`penaltyshootoutcontroller.cpp:417`) sees period 9 and `ApplyPostMatchForfeits` (gate `match.cpp:638`) does nothing. `PeriodAfterWhistle` leaves 8 unchanged (`pesclock.cpp:105-112`).
6. **Why the test fails on `aea2e6ce`.** The level-path code (`referee.cpp:317-328`, `matchprogression.cpp`, `pesclock.cpp`, `match.cpp`, `penaltyshootoutcontroller.cpp`) is byte-identical between `7954923d` and `aea2e6ce` (`git diff` empty). The difference is the in-play score: home scores two real goals on the new tip (DIAG in `pes-port-StepAudit5` section 6: HOME 2, AWAY 2). The test does not pin the home score, so the regulation score is level and GF takes its knockout route.
7. **GF comment to correct.** `match.cpp:635-637` says PES's writer "runs every tick of the post-match period". The asm does not show that cadence (caveat below). The comment is unverified.

## PES asm chain (VA; decoded this round unless marked)

- Period-end programme builder `FUN_1404505D0`: the non-full-time branch queues steps `0x45, 0x25, 0x24, 0x0F, 0x1B`. Step `0x13` (min-players) appears only in the period-8 branch (`MI+0x1168 == 8`), which also queues `0x29`. Full-time branch (`== 10`): `0x26`, `0x24`.
- Whistle period writer `FUN_1404494E0` (sequencer step `0x1C`): when the phase `49C0 == 7`, the next period is `FUN_1404493D0` (period 3), `FUN_1404499D0` (period 7), or the current period otherwise. It is stored by `FUN_14102CAD0`, a plain store `*(flow+0x2790) = value`.
- Tie test, period 3, `FUN_1404493D0`: side totals `Σ k=0..7` from `FUN_1408C8FC0(side, id 0, mode 5)`, read from `side + 0x59530 + k*0x20` (guarded by an FNV check at `+0x59530`; mismatch calls `FUN_1415A1714`). Aggregate tallies `E+0x6A` / `E+0x6B` are added to the side totals. If `E+0x68` is set, the tie is a doubled-tally comparison (away-goals style). Tie: `E+0x66` gives `FUN_140443E30` and 4; else `E+0x67 == 0` gives 10; else 8. Not tied gives 10.
- Tie test, period 7, `FUN_1404499D0`: counters k=6 and k=7 per side. If `E+0x69` is set, the side-1 total is doubled in the comparison. Tie gives 8 if `E+0x67`, else 10. Not tied gives 10.
- Table: `E+0x66 = 0, E+0x67 = 0`: level gives 10. `E+0x66 = 0, E+0x67 = 1`: level gives 8 (shoot-out, no extra time). `E+0x66 = 1`: level gives 4 (extra time).
- Writer 1 `FUN_140A23260`: gate `MI+0x1168 == 10`. For on-pitch slots (`MI+0x1748`), id via `FUN_140A3AE30` / `FUN_140A3AF30`, validity `FUN_140A3B520`, record `FUN_140A73E20` (= obj+4). If rec+0x14 == 0 and rec+8 == 4: per-player record `+0x1B70 = 1`, `+0x1B74 = 3`. Caller: `FUN_140A22130` calls it unconditionally (`140A22294`). `FUN_140A0D190` calls `FUN_140A22130` once per entry of the event store, and only while `MB+0x2048 == 0`. So writer 1 is not a per-tick call. Whether the store is non-empty and the state is 0 in the post-match period is not traced. (`FUN_140A0D190` does increment `MB+0xBD00` on each call.)
- Forfeit record (`FUN_1408C7210`, round 5): per-player base `side + 0x64D8 + id*0x2134`, flag `+0x1B70`, score `+0x1B74`. The tie tests read only id-0 side counters (`+0x59530..0x59630`) and the `E` tallies, so they never read these bytes. Writer 2 also writes `+0x18ED` / `+0x18F4` under `MI+0x1574 == 1`; the tie tests do not read those either.
- Minimum players, `FUN_140447B50` case 6 (step `0x13`): for each side, `FUN_140A3BFC0(MI, side) < E+0x17` (7, GF stand-in) gives a flag. If any flag: abandon result to `flow+0x2AA8` / `+0x2AAC`, `FUN_14102CAD0(flow+0x2790, 10)`, phase `FUN_14102CAD0(flow+0x49C0, 7)`, stop reason `FUN_141B8B250(flow+0x49C0, 2)`. Step `0x13` with stop reason 2 also calls `FUN_1404505D0`. The count function `FUN_140A3BFC0` is not decoded here.
- Values: 9 = shoot-out and 10 = full time are CONFIRMED values with INF meanings (`pes-re-A6Writers` section 5). The 8 to 9 and 9 to 10 writers are OPEN.

## Real-call evidence

- None for a level regulation end with a red-state man. No live read of `E+0x66` / `E+0x67`.
- Shoot-out captures exist, labelled "shoot-out, periods 8-10" (`docs/ports/pes-port-Captures.md`, rows `captures_f/g/h`). They show real shoot-out play in periods 8 to 10. The 9-to-10 writer itself is not located.

## What the test should assert instead (for Main to decide; test NOT changed)

- A. Keep GF's knockout stand-in (extra time and shoot-out after a draw). Then the faithful assertion is: level gives `1stExtraTime`, `2ndExtraTime`, `Penalties`, and the 3-0 is recorded after the shoot-out at period 10. Needs (i) a window that covers extra time and the shoot-out, (ii) GF's clock at 10 before the shoot-out's `GameOver` (currently 9), and (iii) the red man still flagged on the pitch through the shoot-out (OPEN; not traced).
- B. Make the match non-knockout (`E+0x66 = 0`, `E+0x67 = 0`). Then the current assertion (3-0 at full time, no extra time) is PES-faithful and needs no ordering change. Needs the `E+0x66` / `E+0x67` source, which is not located, and a GF switch in `PeriodAfterWhistle` and `GetNext`.
- Either way, pin the premise: the test must not depend on the home side's in-play goals. Assert the route explicitly, or set the score at the whistle.
- Do not move the forfeit ahead of the level decision: PES does not do that.

## Open

- Source of `E+0x66`, `E+0x67`, `E+0x68`, `E+0x69` and `E+0x17` (the settings writer; GF's 7 is a stand-in).
- The 8 to 9 and 9 to 10 writers (`pes-re-A6Writers` OPEN).
- Whether the red man keeps the on-pitch flag through the shoot-out.
- Writer 1's cadence: the event-store contents and brain state `MB+0x2048` in the post-match period.
- Other restart builders that carry step `0x13` or `0x1C` (not enumerated).
- Consumer of the abandon result `flow+0x2AA8` / `+0x2AAC` (not traced).
