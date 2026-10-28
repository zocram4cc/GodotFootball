# pes-port-OwnerPickup: designated owner from the ring-0 scan (MA+0x2600C), gate 4c and the 0x5D gates decoded; stall still red at 147 (28-10-26)

**Status.** `pes_stall_test` is NOT green. `PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker` still fails with 147 live breaker firings, the same count as at `b095afa4`. What landed is the decoded designated-owner scan, gate 4c, and the two 0x5D branch gates, with tests. The 0x5D clip is not wired, because its action class is undecoded. Branch `StoppageFlow`: `785ef0be` (merge of `origin/main`) then `8eee7bfb` (port). Both confirmed with `git ls-remote origin StoppageFlow`.

## 1. Commits
- `785ef0be`: `git merge origin/main` (`0ccc5bc2`). The `extern/godot-cpp` symlink was moved aside for the merge and restored afterwards (same method as `260faa54`).
- `8eee7bfb`: the port (six files, below).

## 2. What is decoded and ported (CONFIRMED unless marked)

### 2.1 Designated owner, MA+0x2600C (FUN_140457070, ring-0 scan)
Source: TrapPick scratch `d_a.c:1248-1283` (the Ghidra bridge on 127.0.0.1:8080 was down, so the decompile is the scratch file, not a live read).
- Loop over `k = 0..8` on `FUN_140A3E190(E, k)` (= `NewestOrRing0(k)`), breaking early when the owner changes.
- If `MI+0x1168 == 9` (the shoot-out, `pes-re-A6Writers.md` §5): the first record with kind 5 names the owner and the scan stops. Otherwise, for each record with kind ≠ `0xE`: if payload+0x1C == 6 the owner is that record's actor; then the scan stops when the record's owner differs from the previous `MA+0x2600C`.
- `FUN_140A62E90(teamRec, slot)`: team outside 0..1 gives `0xFF` (none), otherwise the roster player (`FUN_140A3B320`). Ported as `ActorKey::Valid()`.
- Port: `pes::DesignatedOwnerScan` (`pesreceive.cpp`), `Match::RecomputePesDesignatedOwner()` (`match.cpp`, each tick when `pesLoop`), stored in `Match::pesDesignatedOwner` (= MA+0x2600C). `period9` is `matchPhase == e_MatchPhase_Penalties`.
- CONFIRMED: the scan body. INF: GF's `NewestOrRing0(n)` matches `FUN_140A3E190(E, n)` (its own doc says so).
- Not ported: the carrier MA+0x2601C (`FUN_140457070` second store). It is already `RecomputePesHolder` (ring 1).

### 2.2 Gate 4c (FUN_1409FA520, `d_b.c:91`)
`if (*(MA+0x2600C) == me) return false`. Ported as `RefusedAsDesignatedOwner(designatedOwner, me)`. It replaces the newest-holder stand-in (ponytail: "FUN_140457070's ring-0 scan is not decoded", now decoded). Restart-taker refusal kept in `_ReceiveCandidate` and labelled as gate 4d's approximation (MI+0x11C8/+0x11CC not ported).

### 2.3 Kick branch 0x5D (reaction entry 3, FUN_1409F8220, `d_a.c:610-662`)
- Order: `FUN_1409FA400` (10 m, the 100 m² table) then `FUN_1409FBB40` and `FUN_1409FBBA0`, then the 0x5D clip, then the TRAP branch `FUN_1409F8640`. The 0x5D branch does NOT pass through `FUN_1409FA520`, so gate 4c does not refuse it. The designated owner therefore takes a slow ball through 0x5D, not TRAP.
- `FUN_1409FBB40` (`d_b.c:164-179`): false if the anime action pointer is null; if AI stage ≠ 0 it returns `stage == 5`; otherwise action byte `& 0x7F == 0x4A`. Port: `KickBranchActionGate`. The decompile's `byte < 0xF8` test is redundant (0x4A and 0xCA are the only bytes with those low seven bits).
- `FUN_1409FBBA0` (`d_b.c:186-223`): ball sample at 10 ticks, height `≤ 0x1425A9BD4`; then the 2D difference of the player sample (`FUN_140A6C6D0`, scaled by `0x14259BDD0 / gs`) and ball sample (`FUN_140A57450`, index 1) squared `<` (`0x14263E398 / (gs · 0x142598590)`)². Port: `KickBranchBallGate`. Sample units INF (see §5).
- Install constants, read by script from `pes_full.bin` (offset = VA − 0x140000000), not typed:

| VA | f32 bits | value | use |
|---|---|---|---|
| `0x1425A9BD4` | `0x3E5E96BB` | 0.2173719 | 0x5D ball height limit (10 ticks) |
| `0x14263E398` | `0x47147000` | 38000.0 | 0x5D speed numerator (38 km/h as m/h) |
| `0x142598590` | `0x45610000` | 3600.0 | 0x5D speed divisor (s/h) |
| `0x14351AE50` (.data, u32) | `54` | 54 | gs |
| `0x14259BDD0` | `0x3F31C71C` | 0.6944444 | player-sample scale (over gs) |
| `0x1425CCA3C` | `0x431D8000` | 157.5 | 0x5D facing limit (NOT ported, see §5) |

- Constants loaded in `LoadReceiveConstants` (`pesreceive.cpp:121-123`).

### 2.4 Not wired: function 0x5D itself
`FUN_1406B4580(anime, 0x5D, ball, out)` picks the clip (virtual `+0x68` of the action at `anime+0xC40+0x1528+8·0x5D`). On success the reaction sets state `+0x874 = 3` and function `+0x878 = 0x5D`. The action's RTTI class and clip family are NOT decoded: the trap-windows table (`pes-re-trap-windows.md` §6) lists ids 0x17, 0x4A, 0x4F and 1..6 only, and the action object for 0x5D is filled at runtime. So `elizacontroller.cpp kReceive` cannot issue it faithfully yet. I did not guess a GF function type.

## 3. Changes (commit `8eee7bfb`)
- `src/sim/onthepitch/pesreceive.{hpp,cpp}`: `Ring0Entry`, `kRing0ScanCount`, `DesignatedOwnerScan`, `RefusedAsDesignatedOwner(ActorKey, ActorKey)`, `KickBranchActionGate`, `KickBranchBallGate`; `ReceiveConstants` gains `kickMaxBallHeight_m`, `kickSpeedNumerator`, `kickSpeedDivisor`. `OwnerInput` (stand-in) deleted.
- `src/sim/onthepitch/match.{hpp,cpp}`: `GetPesDesignatedOwner()`, `RecomputePesDesignatedOwner()`, member `pesDesignatedOwner`. The recompute runs before the designation block (`match.cpp:927`).
- `src/sim/onthepitch/player/controller/playercontroller.cpp` `_ReceiveCandidate`: gate 4c reads the decoded owner; the newest-holder stand-in is removed.
- `tests/sim/pes_receive_test.cpp`: 14 new tests replace the 4 stand-in tests (8 owner-scan/gate-4c, 6 0x5D gates).
- Left unchanged on purpose: `designatedPossessionPlayer` (the GF chaser stand-in) and the breaker's nearest-man designation. GF dereferences `designatedPossessionPlayer` without a null check in several AI paths, while the PES owner is `NoActorKey()` on a loose ball. So the PES owner is a separate value used only by gate 4c.

## 4. Verification
- **Red.** I wrote the new tests before the implementation but did NOT build them red first. The first build was with the implementation in place. That breaks the TDD order in the brief; stated plainly.
- **`pes_receive_test`** (with `PES21_ROOT`): 40 passed + 9 failed in the mutation run (49 total, including the 3 install tests, which pass when `PES21_ROOT` is set). After the mutations were reverted, the sweep ran it with exit 0.
- **Mutations.** Batched: all applied in one edit and one rebuild, then reverted. Each failing test maps to a mutation:

| # | mutation (in `pesreceive.cpp`) | failing test(s) | result |
|---|---|---|---|
| M1 | period-9 branch disabled | `InPeriodNineTheFirstKindFiveShot…` | killed |
| M2 | `0xE` skip removed | `AnEventOfKindEIsSkipped…` | killed |
| M3 | early `break` removed | `TheScanStopsWhenTheOwner…` | killed |
| M4 | roster check removed | none | NOT killed alone: masked by M1 in the batch; not re-run individually |
| M5 | tag test inverted | `ATaggedNewestTouch…` | killed; `AnUntaggedTouch…` was masked by empty-slot resets, so the test was moved to the last slot. That fix was not re-mutated |
| M6 | `me` comparison dropped (gate 4c) | `TheDesignatedOwnerIsRefused…` | killed |
| M7 | AI-stage gate removed | `AHeldAiStageDecidesAlone…` | killed (also implicated by M8) |
| M8 | bit 7 not masked | `ActionFourAAdmits…` | killed |
| M9 | height gate removed | `ABallAtTheHeightLimit…` | killed |
| M10 | speed `<` → `<=` | not run (budget) | NOT run |
| M11 | speed gate removed | `TheRelativeSpeedMustBe…` | killed |

- **Reverted.** The batch was reverted and the untagged test moved; the sweep below ran on the reverted code.
- **`pes_stall_test`** (`PES21_ROOT`, slots 2-3, `GF_SIM_SLOTS=4`): `ALiveBallNever…` FAILED, 147 live firings (ball 0.08 m/s). `TheBreakerNever…` passes. Two runs: the port (147) and the probe below (147).
- **Probe (reverted).** Skip the breaker's closest-man designation on the PES path (`match.cpp:972`): still 147 firings, identical. So the designation is not what gates this stall.
- **Sweep**: all 96 `build/tests/**/pes_*` binaries, slots 2-3 (`GF_SIM_SLOTS=4`), `timeout 1800` (3600 for `pes_restart_delay_test`), after a full `cmake --build build`. Result: 95 exit 0, 1 red: `pes_stall_test` (1 of 2). No timeouts. Wall 5685 s. Gate: not run (Main gates). Smoke: not run.

## 5. Why the stall is still red (diagnosis, not fixed)
- CONFIRMED: at each firing the forced man is the loose-ball presser (A4 19 / A3 18), 0.4–0.7 m from a 0.078 m/s ball. The census shows no TRAP queued on the sampled ticks (see `pes-port-StoppageFlow5.md` §2).
- CONFIRMED: changing the designation on the PES path does not change the count (probe). So the presser's failure is in the receive path, not the designation.
- OPEN: which gate of `_ReceiveCandidate` / `_TrapCommand` returns false for the presser (`WithinBallDistanceTable`, `RetouchAllowed` lockout, `ShotRunsThrough`). Not instrumented.
- OPEN: a PES TRAP needs a contact frame. First-touch §7.6(b) says GF sets `touchFrame = -1` for PES clips (`humanoid.cpp:2787`), so even a queued TRAP would not touch. Not rechecked here.
- OPEN: the 0x5D branch's facing gate, which is CONFIRMED in the decompile but NOT ported: when the command class `*(param_2+0x8D0) ≠ 0`, the difference of `*(param_2+0xE48)` and `FUN_140439F90(...)`, wrapped to [−180, 180], must be `< 157.5°` (`0x1425CCA3C`), else the 0x5D branch is skipped (`d_a.c:619-653`). Needs `FUN_140439F90` and the `A+0xE48` field.
- INF: the 0x5D speed sample units. `FUN_140A6C6D0` scales the player sample by `0x14259BDD0/gs`, so the unit of its output is not known. The gate takes the two samples as PES writes them.

## 6. Open items (numbered, with the method)
1. Decode the action at id 0x5D: class (RTTI) and clip family. Method: Main's headless decompiler (`~/ghidra-projects/scratch/Unpacker/dec.sh OUT.txt 0xVA`, about 16 s) on the constructor/writer of `anime+0xC40+0x1528+8·0x5D` and on `FUN_1406B4580`/`FUN_1406D9010`. Then wire the 0x5D branch into `elizacontroller.cpp kReceive` for the designated owner, with the facing gate (item 4).
2. Find which `_ReceiveCandidate`/`_TrapCommand` gate refuses the presser on the stall. Method: a live-free probe in the test harness that logs each gate's return on the 147 ticks.
3. PES touch frame for TRAP clips from the HitData record (first-touch §7.6(b)). Needed before any TRAP can make contact.
4. Port `FUN_140439F90` and the 0x5D facing gate (157.5°).
5. Units of `FUN_140A6C6D0` and `FUN_140A57450` (for the 0x5D speed gate).
6. Re-run M4, M5 (with the strengthened test) and M10 individually.
7. Decide whether the chaser stand-in (`designatedPossessionPlayer`, time-ratio and breaker) should be replaced on the PES path. Brief item 2 asked for it; I did not, because GF dereferences it unguarded. The PES owner is `NoActorKey()` on a loose ball.
8. Period 9 is treated as `e_MatchPhase_Penalties` (the shoot-out, per the decode). INF that the GF phase enum maps one to one.

## 7. Deviations from the brief
- Brief item 2 (replace the designated-possession block) only replaced the owner that gate 4c reads (item 7 above).
- Brief item 3 (wire 0x5D into `kReceive`) not done: the clip family is undecoded (item 1). The gates are ported and tested.
- Brief item 4 red-first: tests written first, red not observed (§4).
- Mutations were batched (§4); M4, M5 (strengthened test) and M10 are not individually verified.
- Brief line references were stale: `match.cpp:809-850` is now the referee stage; the designation block is at `match.cpp:~920-950` at `785ef0be`.
- Ghidra bridge on 127.0.0.1:8080 was down during this work; decompiles came from the TrapPick scratch files.
