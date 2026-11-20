# pes-port-StallLive7: the PES carrier rule committed; the press gate on the carrier; live-ball firings 223 (StallLive, 20-11-26)

Status: PARTIAL. Branch `StallLive` at **b96a4043c6fc1eba5fc02b3c8db80b00cbc57abf** (ls-remote confirmed; parent 93ad88d1). Acceptance not met: `PesStall.ALiveBall` is red at 223 (base 606, the 93ad88d1 measure 36), `TheWorldHolderFieldIsThePesHolderEveryTick` is red (30 of 59,194 side-ticks), the mutation on the kick clear is not done. Build dir deleted. PES pid 2998562 was used read-only (samples taken before the relaunch; Main told it can relaunch).

## 1. Commit hygiene (CONFIRMED, my error)
The round asked for split commits. I drafted three, but my `take` calls did not narrow the draft's default selection, so **b96a4043 contains all five paths** (match.hpp, humanoid.cpp, teamAIcontroller.cpp, elizacontroller.cpp, tests/sim/pes_stall_test.cpp) under the subject "PES carrier: a kick clears MA+0x2601C, a pass target's touch takes it". Its subject describes only the first two hunks. It is pushed; the owner rule forbids rewriting it, so it stays, and this report is the honest description. The second and third commits never landed (their draft call failed after the first commit).

## 2. What the branch does now (CONFIRMED by the code)
- **Kick clears the PES carrier** (match.hpp `RecordPassEvent`: `pesKeyActor = nullptr; pesHolder = nullptr`). Measured in StallLive6 section 2: carrier 0xFF through every kick flight.
- **A pass target's touch takes the carrier** (humanoid.cpp touch gate: `AI_HasPossession(...) || team->PassGetReceiver() == CastPlayer()`). Measured: the receiver's touch sets the carrier to him and clears T+0x78 on the same frame.
- **The loose-ball press writer and the press approach run on the PES carrier** (teamAIcontroller.cpp `PesLooseWriterTick`: `match->GetPesHolder() != nullptr` instead of `GetBallRetainer()`; the GF-only owned-man loop removed; elizacontroller.cpp press approach gate `!match->GetPesHolder()`). PES: the writer runs only with no carrier (pes-re-approach.md 1.1; first-touch 7.1).
- The test dump prints the forced man's presser state (STALLPRESS).

## 3. Measured on b96a4043 (CONFIRMED runs)
| Check | Result |
|---|---|
| `PesPassTarget.EveryPassInFlightHasItsOwnSidesReceiver` | **PASSED** (green) |
| `PesHang` (seed 11, 2 min, full time) | **PASSED** |
| Seed-7 2-min pes-loop smoke | RC 0, **29,597 sim ticks** to full time (normal range; 136,465 was the round-4 stall) |
| `gen_translated.py --check` | byte-identical, 10,150 functions |
| `ledger.py --check` | current, 47,642 rows |
| `PesThrowInRelease` (9 tests, 2,712 s, under the cap) | 8 PASSED; `TheThrowInTakersAnklesStayBehindTheLine` FAILED: **6 of 211** taking ticks short, worst **0.0269 m** (was 2 of 109, 0.0155 m, on earlier branches; the seed list changed in the tests, not chased) |
| `PesStall.*` (full suite, 4,000 s cap) | PASSED: ATrapClipIsNever..., AHolderIsNeverHandedANewTrap, TheBreakerNeverForces..., ANoManIsEverHandedNoCommandAtAClipEnd, ATrapTouchIsTheBodyScansGrantAndNothingElse (StallContact's), ATrapReachesABallAtRest. FAILED: **ALiveBall** (223), **TheWorldHolderField...** (30 of 59,194 side-ticks) |

## 4. The live-ball firings (CONFIRMED census with presser state, 223 firings)
- All 223 have no PES carrier (`pesHolder=-1`).
- 51 firings: the forced man is the side's PRESS presser himself (`forcedIsPresser=1`), so the approach does not reach the ball in 5 s.
- ~172 firings: a presser exists on the forced man's side but a non-presser is the nearest man (`forcedIsPresser=0`, `sidePress=1`). The breaker forces the nearest man while the presser is still approaching or has not been given a path.
- Distances of the forced men: 94 under 1 m, 115 at 1–2 m, 14 at 2 m or more. Clips: MOVE walk/idle (fn 14, ac 2) dominate; a few slide and trip.
- Interpretation (INF): the press approach is now issued on a 255 carrier, but the presser's body is steered by the GF clip gate (`pesmovestep.cpp:620`, IsMovementClip) and does not close the distance (pes-re-move-gate.md; the same G2 finding as round 1). That is the next port.

## 5. Not done
- **Kick-clear mutation** (one mutation required): not run. The pass-target test is the red that a removed kick clear would show (measured earlier: receiver null without the clear), so that is the planned mutation.
- **Holder-field test**: red (30 side-ticks). Cause: the kick clears the holder in `RecordPassEvent` after `team.cpp:RunPesPassTarget` has already written MA+0x2601C in the same tick, so MA shows the old holder for one tick. Candidate fix: in `RecordPassEvent`, under `if (PesLoop())`, `pesWorld->SetMaOwner(s, pes::kMiNoSlot)` for both sides. I wrote it, did not measure it (budget), and reverted it. Branch is not carrying it.
- **Presser approach** (the 223): the MOVESTEP gate for a PRESS man on a GF clip (pes-re-move-gate.md). Not ported.
- **Split commits**: see section 1.

## 6. Cross-check with StallContact
StallContact measured the two hunks on its tree (46aaf971 base plus the hunks, uncommitted): census 606 -> 1498 and smoke idle episodes 7 -> 22. On my tree the same hunks plus the press gate give 223 (below 606), so the regression is tree-dependent; I did not ablate one hunk (a single-hunk run is ~25 min and was not in budget). StallContact keeps its hunks uncommitted.

## 7. Next step (one line)
Port the PRESS man's approach so his body follows the planner: the MOVESTEP movement-clip gate for a GF clip under a PRESS command (pesmovestep.cpp:620 and humanoid.cpp:604-613), then re-measure the 223 and the 51 presser firings.
