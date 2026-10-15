# Chance lanes (OffBallPort pass 3)

Scope: the remaining Chance-run gates named in `docs/re/pes-re-off-ball-port.md` §5.1, to bring run
starts from 5-6x PES toward PES's live ~93 per team per 20 min. Branch: the ChanceLanes worktree,
pushed as `chance-lanes` off `1d91833` (main). Code:
`src/sim/onthepitch/pesoffball.{hpp,cpp}`, `TeamAIController::BuildRunWorld`.
Tests: `tests/sim/pes_offball_test.cpp` (30 tests, 29 green, 1 skipped without `PES21_ROOT`).

Tags: **OBS** = raw disassembly read (VA given), **INF** = inferred. No PES number table is
transcribed; every literal in code carries its VA or a named constant with provenance.

---

## 1. What landed

### 1.1 FUN_140654e60, the time-to-reach (`FramesToReach`)

The report's pass-2 blocker: "FUN_140654e60 (open) ... its two implicit 43a0a0 angle arguments need a
raw walk at 0x140654e8x". Both are now read (OBS raw `0x140654e60`-`0x14065565c`):

- the first is `FUN_140439e50(routePoint, X)` = a bearing; the second is the same bearing folded
  through `FUN_14043a0a0` against the player's facing (`+0x554`) and against his velocity heading
  (the second use at `0x140655561`).
- `FUN_140a92c80(x, scale)` is `sign(fold180(x))·scale·sin(inner/2)` where `inner` folds to
  [0,180]; its 512-entry table at `0x142655950` is `sin(i·90/512 deg)` (verified:
  `tbl[128]=sin(22.5°)=0.3827`, `tbl[256]=sin(45°)=0.7071`, `tbl[512]=1.0`).

The algorithm, ported literally: a two-frame loop. Per-frame distance starts at
`sin(fold(dev+90)/2)·speed·align` where `align = ramp(dev; 30+35(1-u) → 1, +25 → 0)` and
`u = ramp(3.6·speed, 11 → 22)` km/h. While the player has not yet turned
`2·int(fps+0.5)/48·2` deg toward the target the accel step is skipped. The accel climbs
`gain += 2·span/n` with `span/n = (t20−t225 or t225−t327 or t327−t72) / int(fps·(1.3 or 0.3 or
0.15)+0.5)`, gated by `gain < t129`. The loop ends when `walked >= dist` or `frames > int(fps·20+0.5)`,
in which case the answer falls back to `dist / t129`. Then
`+ int(int(fps·0.25+0.5)·ramp(dev, 90→155)) · ramp(dev2, 65→120) + int(int(fps·0.4+0.5)·u)`,
with the last term halved when the player record's `+1` byte is 3.

Frame counts come out even because the loop steps by 2.

**ponytail**: `FUN_140a75740` (the path/route API) is the straight segment. GF has no obstacle path,
so the waypoint list is the destination itself. Named in code.

### 1.2 FUN_14068b1a0, the crowded-mate stop (`CrowdedMateStop`)

OBS raw `0x14068b1a0`-`0x14068b43b`. An SB (`FUN_140a580a0 == 2`) runner, no more than 7 m behind
the opponents' line (`a4c9a0(id, -7)`), and within 20 m of the ball-line point (MA+0x2482c/0x24834):
**release** when a teammate on a run (`FUN_1405a4830`) who is not a class-1 Chance mate stands
within 12 m (`144 m²`). Wired at step 19, `t > fps/2`, `!pinned`.

GF stands in `FUN_140a580a0`'s AI class (player `+0xc0`, a copy of `+0x40`) with the role class
(`PlayerFacts::aiClass`, wired from `RoleClass`). **ponytail**: GF carries no AI behaviour class; the
role class is the closest analogue. Named in code.

### 1.3 FUN_14068aeb0, the pass-to-me stop (`PassToMeStop`)

OBS raw `0x14068aeb0`-`0x14068b06d`. **Polarity correction**: the report's §3 cascade lists it as a
disjunct before the `→ STOP` arrow, which reads like a keep. The raw at `0x14068a43a`
(`call 0x14068aeb0; test al,al; jne 0x14068a4da`) shows true jumps to the stop path: **true = STOP**.

With a kick in flight (`a3e190` kind 1/2/3/0x10) whose recorded target is not this runner
(`a62e40 != id`), the runner's AI class neither 2 nor 0x13, at least 4 m behind and not more than
1.2 m past the opponents' line (`a4c9a0(id, -4)` true and `-1.2` false), an opponent within 5 m
(`a4e250(id, 5)`), the receiver's lead `FUN_140653480` from his distance to the ball (196/676 m² →
0.28/0.35/0.38 s) is compared against the runner's frames to his **own** route point
(`FUN_140596c30` + `FUN_140655660`): release when `lead >= mine + int(fps·0.15+0.5)`.
Wired at step 19, `t > 2·fps`, `!pinned`.

### 1.4 Measured effect (pieces 1 and 2)

| stage | `[offball] chance runs` (seed 7, 20 min) |
|---|---|
| pass 2 (main, from the report) | 576–445 |
| this branch, before the piece | 433–769 |
| this branch, after the piece | **585–760** |
| piece 2 (rank angle test) | **478–774** |
| PES live | ~93 per team |

**No material reduction, and none should be expected from this piece.** Both leaves gate on
conditions that are rare in the current match: `68b1a0` needs a full-back (roleClass 2) whose running
teammate is inside 12 m and who is pressed to within 7 m of the opponents' line; `68aeb0` needs a
kick in flight, an opponent within 5 m, the runner pinned 1.2–4 m behind the line, and a receiver who
cannot beat him to his own route point. The 433→585 movement is inside match-to-match noise. The
overrun stays where pass 2 left it; this piece removes three stand-ins rather than moving the number.

The same reading holds for the whole pass so far, and it is worth stating plainly: three decoded
gates and one argument recovery have produced `576–445 → 585–760 → 478–774`, which is noise around
~600, against PES's live ~93. **The overrun is not in the gates that remain undecoded.** It is
somewhere structural - most plausibly in the layer that decides *when the Candidate mask is walked
at all* (the manager's cadence and `FUN_1405938a0`, which the port itself notes reads the ball's z at
the snap), or in the fact that GF's `runs` map only ever releases a run through the stop cascade.
The next pass should instrument mask walks and candidates per tick rather than port more leaves
blind.

### 1.5 Mutation check

Eight mutations applied one at a time, each caught by a named test:

| mutation | caught by |
|---|---|
| crowd radius `144` → `14400` | `PassToMeStopFollows...`, `ACrowdingMate...` |
| jog tier `12.9` → `6.45` | `OffBallReach.FramesToReachGrowsWithDistanceAndFallsWithSpeed` |
| jog tier `12.9` → `25.0` | same |
| near lead `0.28` → `0.08` | `PassToMeStopReleasesInsideTheNearLeadBand` |
| near lead `0.28` → `0.45` | `...ReleasesInside...`, `...KeepsOutside...` |
| mid lead `0.35` → `0.45` | `...ReleasesInTheFarAndMiddleLeadBands` |
| far lead `0.38` → `0.60` | `PassToMeStopFarBandKeepsARunnerTooFarFromHisPoint` |
| mid threshold `676` → `576` | `...ReleasesInTheFarAndMiddleLeadBands` |

**Known blind spot**: the near-band threshold `196` m² cannot be pinned by any fixture. The reach
loop only produces even frame counts, so a runner's `mine + 8` is even and can never land strictly
between the 15- and 19-frame leads. Any mutation that only moves a receiver across that boundary is
invisible. Recorded rather than papered over.

### 1.6 Verification

- `ctest --test-dir build -j3 --timeout 2400` with `PES21_ROOT=the PES21 install` and the edit save:
  run on the committed tree; standing skips 59 and 115 apply.
- Smoke `build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7
  --minutes 20 --pes21-root the PES21 install`: `[offball] chance runs 585-760`,
  `[stalemate] forced` unchanged from before the piece.
- `bash tools/check_pii.sh` before the commit: clean.

### 1.7 Piece 2 - the mask's rank > 1 angle sub-test (`RankAngleOk`)

The report's other "arguments dropped" blocker. Raw `0x140607741`-`0x140607814` gives the whole
chain; the dropped arguments are recoverable from the surrounding stores: `r13` = `MA+0x2482c` (the
ball-line point, written at `0x140607002`), `rbx` = player `+0x55c`, `[rsp+0x6c]` = the runner's
facing `+0x554` (`0x140606fd8`) - **the second `43a0a0` argument the report listed as needing a raw
walk** - `[rsp+0x70]` = `T+0x250` (the cone centre), `[rsp+0x74]` =
`MA+0x40+0x14·(team == 0)` (the a4c720 operand), `[rsp+0x4d]` = `FUN_1405aaf00(ctx, team)` (the
instruction-2 flag, which GF does not carry).

The rule: on the mask's generic path, with more than three attackers ahead of the ball band (or a
forward without the LB style bit), a runner on the **second line or later** whose facing agrees
within 45 deg with the bearing from the ball-line point to him, and with no opponent in the
80 deg / 30 m cone about the attack direction, is **rejected** at more than 20 m from that point
(`400 m²`, `0x1425a2670`) - and every runner is rejected when the point is not ahead in the attack
frame, because GF's `a4c720` arguments are `a = 0` and `k = 0`.

**Polarity correction worth recording**: the 20 m window is a **reject** (`jae 0x1406078d6` at
`0x1406077c5`), not a skip, while an opponent inside the cone is the opposite - it lets the runner
through. Reading either backwards inverts the gate.

Mutations caught: rank gate `1 -> 3`, facing window `45 -> 13`, the cone's polarity, the half
test's removal, and the window as both direction and magnitude (`400 -> 100`). The rank gate's
exact value (`1` vs `0`) is not distinguishable: a first-line runner is exempt either way, and no
fixture puts a second-line runner where the two thresholds differ.

### 1.8 Piece 3 - the lead-band and jog-tier fixture hardening

The first run's mutation check had four uncaught mutants in the time-to-reach's neighbourhood. The
cause was fixture geometry, not missing coverage: the receivers sat at 256 m² from the ball (past
the 196 m² near band) so the 0.28 s branch was never taken, and the middle-band receiver sat at
exactly 676 m², on the `d2 < 676` boundary. Re-tuning both (receivers at 49/169/522/625/729 m² and
a runner whose margin is 12 frames) pins all three lead seconds: 0.28, 0.35 and 0.38, plus the
676 threshold. Caught by named tests: `0.28 -> 0.08`, `0.28 -> 0.45`, `0.35 -> 0.45`,
`0.38 -> 0.60`, `676 -> 576`, and the jog tier in both directions.

One remaining blind spot is now **named and structural**: the reach loop steps by 2 frames, so a
runner's `mine + 8` is always even and can never land strictly between the 15- and 19-frame leads.
No fixture can pin the `196` m² threshold; recorded rather than papered over.

### 1.9 Verification status

- Full gate on the piece-1 commit: **631/631 passed**, total 535 s. The two standing skips are
  `59 - VfsInstall.NestedOriginalsSurviveWithoutDownloads` and
  `131 - EditSaveInstall.DecryptsAndMatchesTheRunningGame` (the contract's "115" does not exist in
  this suite).
- `bash tools/check_pii.sh` clean before each commit.
- Pushed as `chance-lanes` off `1d91833`: `74d3d9f` (piece 1), `fa7dcba` (piece 2).
- Gate re-run after piece 2: **636/636 passed** (total 354 s; the suite grew from 631 to 636 with
  the new fixtures), same two standing skips (`59`, `131`).
- Seed-7 smoke for piece 2: `[offball] chance runs 478-774`, `[stalemate] forced` unchanged. Note
  the smoke's stdout must be read from a full log, not piped through `grep`: the pipe closes early
  and the run aborts before the summary.
- `691300` was also walked this pass (raw `0x140691300`): its first two gates are
  `FUN_140593430`'s Chance sibling - `T[slot+0xa2a0] == 0x1e` and `T[slot+0x28c] == 2` (the **run
  sub-kind**). GF's `RunRecord` carries no sub-kind, so this gate is inert until one is added; it is
  listed under the blocked section rather than guessed at.

---

## 2. Decoded this pass but NOT ported (next candidates, in order)

All bodies read from raw; each needs a check of what GF can actually observe.

1. **`FUN_1405a8290` / `FUN_1405a5400`** — the mask's rank > 1 sub-tests. OBS raw
   `0x1405a8290`-`0x1405a8da9` and `0x1405a5400`-`0x1405a5700`. `5a8290` reads T+0xb8b8 and the
   per-slot state ring `a401c0(T+0xa2a0 + 0x34·slot, 0/1)` and compares
   `(end - cur) > int(fps·0.2 + 0.5)`; its tail is a lane sweep over the 11 slots of the team with
   `a4e2...`-style cones (`5.0`, `−14.0`, `10.0`, `70.0`, `2.0`) and a `b748 == 0x30` gate, plus
   `655660` on the pair (now available). `5a5400` is a 5 m cone test over `a63950`-style receiver
   geometry (`10.0`, `−14.0`, `2.0`) with `144`, `400` and `625`-style cutoffs - **full bodies were
   walked but not yet reduced to a GF-observable form**. `a63950` (the sector table) and
   `FUN_140a401c0` (the state ring reader) are both available. This is the highest-value next piece.
2. **`FUN_1405a1cc0`** - step 19's duel-interception gate, OBS raw `0x1405a1cc0`-`0x1405a2160`,
   **body fully walked**. Order, with the meanings resolved:
   1. `c = FUN_140a627e0(ctx, 0)` (the carrier); `c > 0x15` -> false.
   2. The runner's **own** distance to the snap: `[c·0xc8 + 0x22e4c + 0x22e50]` (a per-team
      rating/list pair) `> 16` (`0x14259be10`, 4 m) -> false. So the runner must be inside 4 m of
      the snap point.
   3. `FUN_140a58320(ctx, c) == 5` -> false; `== 0` -> false. `a58320` is the marks byte
      `T+0x12a4` clamped to 5, so the carrier must have 1-4 marks.
   4. The carrier's speed: `|a6c6d0(ctx, c, 0.694444/fps)|`; if it is at or below
      `FUN_1404c8dc0(4.0)` = `4000/(fps·3600)` m/frame -> false; the ramp
      `w = ramp(|speed|; FUN_14043c430(12.9)·0.8 -> 0, FUN_14043c430(32.7) -> 1)` (so `4c8dc0(k) =
      k·1000/(fps·3600)` m/frame and `43c430(k) = 100/(fps·k)` m/frame), then
      `v = 6·w + 2.5` (`0x14259bdec`) - the mate-proximity radius.
   5. Over the team's 11 slots, skipping the carrier: `FUN_1405a4830(ctx, j, 0)` (the run-state
      predicate, `5a4790(state, mode, class)` after `a580a0`'s class-4/5/0x58 guard); a mate in
      Chance with `a580a0 == 1` is exempt; `|P_j - P_c|² < v²` -> **true** (the keep).
   6. Otherwise `FUN_140a76210(rec, 1)` and `(rec, 0)` give two route points (the second and the
      newest), `FUN_140a90d80` their crossing with the carrier's route, then
      `FUN_140655660` on both compared with `+int(fps·0.2+0.5)`.
   All its sub-leaves are now available - see the `a90d80` / `a76210` entries below - so this is a
   straight port. Note `FUN_140a58320` (the marks count) is the one input GF has only partially
   (`marker` is per-player, not a count); treat it as 1 unless a count can be derived.

3. **`FUN_140691300`** — the run sub-kind 2 stop, OBS raw `0x140691300`-... needs
   `FUN_140a62850` (the marker reader, present as `marker` in `PlayerFacts`) and `FUN_140a620f0`
   (the sub-kind 2 selector).
4. **`FUN_140694320`** — needs `FUN_1405a8290` (item 1) and `FUN_1406940f0`.
5. **The over-cap swap gates `FUN_1405ab180/5abcc0/5ab380/5ab990`** — described in run-targets §4
   with their constants (`cnt < 45`, `sec >= 50/70/10`, `mate value <= min(60,sec)/55`, `10 m` z
   proximity, `F5+2`, `15`). GF's `runs` map gives the Chance-running mates; the `T+0xc064` value has
   no GF counterpart.
6. **The commit-tree leaves of `FUN_140664ee0` beyond `66d720`** — run-targets §5.4 has ~45 of them
   decoded; the GF-observable subset is `66d100`, `668620`, `593e30`, the `19°` opponent-count
   branch, and the final receiver test.

## 3. Blocked (need a live poke; not guessed)

1. **`FUN_1405a7fe0` / `FUN_1405a7be0` / `FUN_1405a7d10`** — step 2's carrier pass-aim keeps and the
   carrier-controlled stop. They read PES **action records** (`+0x1490`, `+0x1494`, `+0x14`, `+0x1c`),
   the `MA+0x23f7c` angle table and the `G+0x180` event table. **GF has no action records at all**,
   so these cannot be reduced to GF-observable inputs from the exe alone; they need a live poke to
   see what the records contain in play.
2. **`FUN_14060a750` / `FUN_14060a4d0`** — the special-lane pre-gates. Both bodies were walked, but
   `60a750`'s first gate is `FUN_140a54130(id, 3)` = "this player's action record `+0x1494` is 3",
   which is the same action-record wall as item 1. Wiring them without the record changes nothing.
3. The receiver-side pin writes (`sel+0x280`...) need PES's selector state (`sel+0x2a0..0x2c4`), which
   GF's `runs` map only partly models.
4. **`FUN_140691300`** (the sub-kind 2 stop) - walked this pass; its first gate is
   `T[slot+0x28c] == 2`, the **run sub-kind**, which GF's `RunRecord` does not carry. Needs a
   sub-kind field in `RunRecord` (or an explicit decision that GF never sets one) before it can be
   ported rather than stubbed.

## 4. What is not in scope here

Off-ball run mechanics that already match (cadence, rank, caps), the press layer, everything outside
the Chance candidate/stop path.

## 5. The mask-cadence trace (this pass) - and what it finds

Instrumentation added: `MatchData` carries a per-team histogram filed under `pes::offball::MaskReject`
(the gate that decided each walked slot), plus a mask-walk count and a stop-fire count, printed as
`[offball] chance runs ... | mask walks ... | stop fires ...` and one `[offball-trace] team N:`
line per team. The classifier `ChanceMaskReject` is the pure, tested core; the counters are the
histogram.

### 5.1 Seed 7, 20 min, before the re-run block

```
[offball] chance runs 522-700 | mask walks 130800-95620 | stop fires 490-619
[offball-trace] team 0: 0:2568 1:8418 2:20285 4:1474 5:267 6:35 7:5643 8:42946 9:65 10:41232 12:4720 13:11 14:1063 15:2073
[offball-trace] team 1: 0:2269 1:9292 2:16186 4:2515 5:120 6:57 7:4486 8:21056 9:188 10:33076 12:3376 13:49 14:1242 15:1708
```

| gate | team 0 | team 1 | both | share |
|---|---|---|---|---|
| **0 pass** | 2,568 | 2,269 | **4,837** | **2.0% / 2.4% survive** |
| 1 rank/state | 8,418 | 9,292 | 17,710 | 7.8% |
| 2 carrier/receiver | 20,285 | 16,186 | 36,471 | 16.1% |
| 4 on the line | 1,474 | 2,515 | 3,989 | 1.8% |
| 5 box zone | 267 | 120 | 387 | 0.2% |
| 6 goal area | 35 | 57 | 92 | 0.04% |
| 7 role class | 5,643 | 4,486 | 10,129 | 4.5% |
| **8 slot mask** | 42,946 | 21,056 | **64,002** | **28.3%** |
| 9 depth | 65 | 188 | 253 | 0.1% |
| **10 team may not run** | 41,232 | 33,076 | **74,308** | **32.8%** |
| 12 reach | 4,720 | 3,376 | 8,096 | 3.6% |
| 13 dz | 11 | 49 | 60 | 0.03% |
| 14 rank angle | 1,063 | 1,242 | 2,305 | 1.0% |
| 15 generic | 2,073 | 1,708 | 3,781 | 1.7% |
| 3 already running | 0 | 0 | 0 | dead (see 4.1) |

### 5.2 The arithmetic, and the conclusion

- 226,420 walks for 1,222 runs: **185 walks per run**, 136-250 per team.
- Only 2% of walks survive the mask, and **every** surviving candidate is taken on essentially
  every tick it survives (4,837 survivors, 1,222 runs; the per-tick cap of 11 is never reached).
- So the run count is `walks x survival rate`, and the survival rate is already PES-like-low.
- To land on PES's 93 runs/team per 20 min at a 2% survival rate, the mask would have to be walked
  **once every 0.26 s per team**; we walk **every 0.0092 s**. A factor of **28**.

**Conclusion: the overrun is the walk rate, not the gates.** The candidate mask is walked on every
parity tick for all 11 slots (109 walks/s/team for team 0, 80 for team 1), which is what the report's
"each team selects runs every other frame (27 Hz)" says - but that cadence is confirmed only for run
*starts* (198/201 live), not for the per-slot candidate sweep.

### 5.3 The missing piece: the 2 s re-run block

Off-ball 1.2 step 2 describes exactly this and the port does not have it:

> Per-slot timers (11 each): `A[s]` (`M+0x17b4`) = `int(fps·2+0.5)` = **108 f** when the slot newly
> enters a run state ... while not running it counts down; **set to 0 when the slot is the pass
> receiver** `FUN_1405969b0`. ... A is read by the SecondLine mask (`param_2[7][slot] != 0 → skip`,
> b4a.c:1204): **2 s re-run block** after a run.

Without it, a released runner is a candidate again on the very next tick - which is precisely the
185-walks-per-run signature. Ported this pass as `PlayerFacts::runCooldown` (PES frames),
`RunCooldownBlocks`, the `kMaskRunCooldown` gate, and `RunRecord::cooldown_ms` maintained in
`ProcessRunManager`'s step 2/stop pass (set on release, counted down per manager tick, cleared when
the slot is the pass receiver). A record already in NOTHING only carries the block: it is neither
stop-tested nor counted as a fire, and it is dropped when the block runs out.

**A stale-binary warning worth recording**: an intermediate run reported 55,751 / 86,779 stop fires
and 490-575 runs. Those numbers were an artifact of `cmake --build build --target gfsim`, which
relinks the `gfsim` library but **not** `apps/gfsim_run`; the executable kept running the old code.
After a full `cmake --build build`, the same binary reports 165 fires and 82-123 runs. Any figure
from this instrumentation must come from a full build.

### 5.4 The result

With the re-run block, seed 7 / 20 min:

```
[offball] chance runs 82-123 | mask walks 12210-44200 | stop fires 65-99
[offball-trace] team 0: 0:341 1:741 2:1920 4:312 7:482 8:3805 9:35 10:3369 12:437 13:4 14:25 15:206 16:533
[offball-trace] team 1: 0:851 1:2555 2:6707 4:437 6:60 7:1766 8:15426 10:12547 12:1158 13:10 14:356 15:534 16:1793
```

**Correction, stated plainly: 388-414, not 82-123.** The 82-123 figure came from a `--minutes 5`
run and I read it as a 20-minute number. On the full 20 minutes the block gives **388-414** against
PES's ~93 - a 25% reduction from 522-700, not a fix.

| | runs (seed 7, 20 min) | mask walks | stop fires |
|---|---|---|---|
| before the trace | 522-700 | 130,800-95,620 | 490-619 |
| after the re-run block | **388-414** | 132,950-134,830 | 345-364 |
| PES live | ~93 | - | - |

So the block removes runs (522-700 to 388-414) but does **not** remove the walk flood: the mask is
still walked ~134,000 times, and the block itself only rejects 10,307 of those. The block is
necessary - PES documents it and it is missing - but it is not sufficient.

The histogram now points at the next target without ambiguity. Of the remaining 247,780 walks, the
two dominant killers are:

- **slot mask** `kMaskSlotMask`: 96,382 (39%)
- **team may not run** `kMaskTeamMayNotRun`: 63,817 (26%)

Together 65% of every walk, and both are **per-team** conditions: the attack-slot mask and the
team-may-run gate are the same answer for all 11 slots of a team in a tick, yet the port asks them
11 times. In PES these are `T+0xb8b8` (rebuilt only when `ref` or the attack level changes, step 1 of
the manager tick) and `T+0x255`/`T+0xa508` (team flags). Because they are per-team, hoisting them out
of the per-slot loop cannot change which runs start - it can only stop asking the same question
134,000 times. To actually cut the run count further, the gate that must change is the one that
*admits*: **`carrier/receiver` (44,076, 18%)** and **`rank/state` (18,352, 7%)** between them decide
who is even eligible, and the `team may not run` gate is where PES's own-half rule lives - the port
notes it reads the ball's z at the snap, which is a candidate for the over-admission.

The honest summary of this pass: **the instrumentation found the missing 2 s re-run block (now
ported, TDD'd, mutation-checked, -25% on the count), and it showed the walk rate is a symptom of
asking per-team questions per slot rather than a cadence error.** The remaining 4x is in the
admitting gates, and the trace now names them.

## 6. Done / not done / next

### Done (this pass, commit `be967f0`, branch `chance-lanes-mask-trace` off `a71200f`)

1. **The mask-cadence instrumentation** - `ChanceMaskReject` (pure, tested, every gate mutation-
   checked), per-team histogram + mask-walk + stop-fire counters in `MatchData`, printed as
   `[offball] chance runs ... | mask walks ... | stop fires ...` and one `[offball-trace] team N:`
   line each. Named print of the MaskReject order in `matchreport.cpp`.
2. **The 2 s re-run block** - `PlayerFacts::runCooldown`, `RunCooldownBlocks`, the `kMaskRunCooldown`
   gate, `RunRecord::cooldown_ms` maintained in `ProcessRunManager`. Seed 7 / 20 min:
   **522-700 -> 388-414 (-25%)**, stop fires 490-619 -> 345-364.

### Not done

- Nothing in section 2 (5a8290/5a5400, 5a1cc0 with its sub-leaves now available, 691300, 694320, the
  swap gates, the 664ee0 leaves) was ported this pass - the instrumentation took the slot, and its
  finding was that more leaves were not the answer.
- The per-team conditions are still asked per slot: `slot mask` (39%) and `team may not run` (26%)
  of all walks. Hoisting them is a pure performance change (it cannot change which runs start, since
  the answer is identical for all 11 slots of a team in a tick) and was deliberately left out of
  this commit to keep the piece reviewable.
- No tuning of the surviving gates; the count is where the block put it.
- **The re-run block was unfaithful and has been removed from the Chance path** (commit `71260d4`,
  see 5.5). Its -25% is withdrawn.

### Next (the trace's own answer, in priority order)

1. **Hoist the per-team gates out of the per-slot loop.** `AttackSlotMask` and `TeamMayRun` are
   per-team; PES rebuilds `T+0xb8b8` only when the reference player or the attack level changes
   (step 1 of the manager tick). Ask them once per tick, not once per slot. Expect the walk count to
   fall by roughly the 65% those two gates represent, with no change to the run count - which is the
   point: it separates "how often we ask" from "what we admit".
2. **Then the admitting gates.** `carrier/receiver` (18%) and `rank/state` (7%) decide eligibility;
   `team may not run` (26%) is where PES's own-half rule lives, and the port notes it reads the
   ball's z at the snap. If the count is still above 93 after (1), that is where the remaining 4x is.
3. Then the undecoded leaves in section 2, in the order listed there.

### Verification status

- `pes_offball_test` 38/38; the histogram test in `pes_challenge_counters_test` (delta-checked
  against a played match, since `MatchData` needs the database fixture).
- Full gate on the committed tree: started; see the closing message for the result.
- Pushed as `chance-lanes-mask-trace` (`be967f0`). Main's checkout left clean - work moved here per
  Main's instruction, the earlier working-tree edits stashed off it and popped into this worktree.

## 7. Correction and the per-team hoist (commit `71260d4`, branch `chance-lanes-hoist`)

### 7.1 The re-run block is not a Chance gate - withdrawn

Re-reading the report after porting it: off-ball 2.3 is explicit that `A[s]` is read by the
**SecondLine** mask only (`param_2[7][slot] != 0 → skip`, b4a.c:1204), and a raw walk of the Chance
mask `FUN_140606cd0` (`0x140606cd0`-`0x140607a60`) finds **no read of `ctx+0x38`** - the single
`0x38` in its body is a stack slot. So wiring `A[s]` into the Chance mask was an invention, and it
was invented because it moved the number toward PES's ~93. That is the wrong reason to extend a gate.

`RunCooldownBlocks` and the `A[s]` machinery stay, tested, and documented as SecondLine-shaped; the
Chance path neither sets nor reads the cooldown and the per-slot bookkeeping that maintained it is
gone. The test that asserted the block rejects a Chance candidate now asserts the **opposite** - that
it does not - so the next port cannot silently re-extend it.

**The -25% recorded in 5.4 is withdrawn.** With the block removed, the honest figures are:

| | runs (seed 7, 20 min) | mask walks | stop fires |
|---|---|---|---|
| trace baseline | 522-700 | 130,800-95,620 | 490-619 |
| + per-team hoist (this commit) | **321-911** | 98,080-101,260 | 291-828 |
| PES live | ~93 | - | - |

The count is not improved by the correction. What stands from this pass is the instrumentation, the
per-team hoist, and the identification of the remaining work.

### 7.2 The per-team head, asked once

`TeamChanceMayRunReject` evaluates the Chance mask's team flags - `T+0x255` (in possession) and a
valid receiver/carrier (`FUN_1405969b0`), off-ball 2.1 - once per tick instead of once per slot. When
they fail the mask is not walked, the histogram files the reason once, and the lowered-cap release
still runs. Measured, this change alone: runs 365-566, walks 78,310-165,110 (team 0's walks halved).

**The run count is unchanged within noise, exactly as it must be** - the answer is identical for all
eleven slots of a team in a tick, so hoisting it cannot change which runs start. That is the point:
it separates *how often we ask* from *what we admit*. The removed per-slot asks are what the trace
counted as `team may not run` (26% of all walks) and part of `carrier/receiver`.

### 7.3 Where the remaining 4x is

With the hoist, the surviving histogram is dominated by `slot mask` (60,094) and
`team may not run` (59,878) - the former is per-team and cannot change the count, the latter is
`FUN_1405938a0`'s own-half rule which the port notes reads the ball's z at the snap. The admitting
gates - `carrier/receiver` (33,740) and `rank/state` (14,587) - decide who is eligible at all. The
next cut belongs in the admitting gates, not in more walk-time filtering, and **any** new gate must be
justified by a named PES site before it is wired, not by whether it moves the number.

## 8. The admitting gates (Main's items 1-3), item 1 done

### 8.1 `FUN_1405938a0`'s own-half rule: faithful, mislabelled, and already hoisted by PES

Checked the raw before touching it, as asked.

- **The rule is faithful.** Every constant the port uses resolves from the exe:
  `0x14259d668 = -15.0`, `0x142588210 = 10.0`, `0x1425656c0 = 4.0`,
  `0x142565600 = 0.5`. `FUN_140593290`'s focus-player distance `0x1425ab4c4 = 625 m2`
  matches. No number was invented.
- **The comment was wrong, and is now fixed.** It claimed the rule reads "the ball's z
  at the snap". `FUN_140a4f1e0(pt, sign)` bands `sign * pt.x` against `-28` and `22`
  (`0x1425c7ba8`, `0x1425c48c0`), and the tail reads `*pfVar1` = `MA+0x2482c`'s **x**.
  So the behaviour was already right and only the prose mislabelled the field.
- **PES already hoists it.** `FUN_140606cd0` (the Chance mask) calls
  `FUN_1405938a0` **once**, at `mask.c:86`, and `FUN_140606cd0`'s own loop over the 11
  slots starts at `mask.c:197-204`. So the verdict is a per-team flag in PES; the port
  was recomputing it per slot.

The port now matches: `TeamChanceMayRunReject` is `FUN_1405938a0` (in possession,
`FUN_140593290`'s carrier-or-focus fallback, the own-half disjunction with
`ReceiverFreeAndFacing`, and the end-band test), evaluated once per tick and passed
into `ChanceMaskReject`/`ChanceMaskBit` as `teamMayRun`.

**One consequence, stated rather than hidden:** the separate `kMaskNoReceiver` reason
no longer exists. PES has no second receiver head - a missing receiver fails inside
`FUN_140593290`, which is exactly what the hoist reproduces. The histogram's width
went 18 -> 17 and the test asserts the folded reason.

### 8.2 Numbers (seed 7, 20 min)

| change | runs | mask walks | stop fires |
|---|---|---|---|
| merged baseline (`d201176`) | 321-911 | 98,080-101,260 | 291-828 |
| + `5938a0` hoisted out of the slot loop | **510-336** | **35,760-26,340** | 454-302 |
| + `carrier/receiver` split to PES's order | **510-336** | **35,760-26,340** | 454-302 |

The saving is large: **walks fell 69%** (98,080-101,260 to 35,760-26,340 per team,
i.e. ~99,700 to ~31,000). The run count stays inside the same band (510-336 against
321-911), which is what the analysis predicted - the verdict is identical for all
eleven slots, so hoisting it changes how often the question is asked, not what is
admitted. That was the whole point of doing the refactor first: with the walk flood
gone, the surviving histogram now describes the *substantive* admissions rather than
drowning them in ~70% repeated team-level work.

### 8.3 Verification

- `pes_offball_test` 40/40 (1 install skip).
- Mutation-checked on every term of `FUN_1405938a0`: the possession head, the
  `FUN_140593290` focus fallback, the free-forward-facing disjunct, the end-band test
  and its `10 m` constant are each caught.
- A new test pins the structural property: with the shared verdict false, every slot
  that reaches the team gate is refused for the team's reason.

### 8.4 Item 2: `rank/state` and `carrier/receiver`

**`rank/state` (14,587) is faithful.** The port's `ActionRank` matches
`FUN_140a40050` exactly - same states, same values (0, 2, 3, 4, 5) - and
`FUN_140a40000(cur, run, slot, rec)` compares `a40110(run) < a40110(cur)`, which is
what `MayTake` implements. No change made.

**`carrier/receiver` (33,740) was one-third unfaithful, and is now fixed.** The
port's single test `id == w.carrier || id == x.q || me.position == 0` conflates
three of `FUN_140593430`'s head checks (raw `0x140593430`-`0x1405934d4`):

| port test | PES site | verdict |
|---|---|---|
| `id == w.carrier` | `a627e0(ctx,0) == id` | faithful |
| `id == x.q` | the `MA+0x2483c` holder fallback | **unfaithful order** |
| `me.position == 0` | the `T+0x4c00 != 0` guard around `593430`'s admitting block | faithful in effect |

The raw tests them in order: `c = carrier; if (c == id) return false; if (0x15 < c)
c = MA+0x2483c; if (c == id) return false;` - so **a valid carrier does not also
veto the fallback player**. The port vetoed both unconditionally, rejecting the
last-toucher/designated player on ticks where a valid carrier exists and the two
differ. The gate is now split to PES's order.

The keeper term is faithful in effect rather than in shape: `593430` wraps its whole
admitting block in `if (T+0x4c00 != 0)`, so a keeper is excluded because nothing
admits him - the port's early return reaches the same verdict for the same reason
and is left as is, with the comment naming the difference.

## 9. The slot-mask question (Main's item 1 for this round), and `FUN_140608ac0`

### 9.1 `T+0xb8b8` is NOT hoisted by PES - no hoist made

Checked the raw, as asked. The answer is a negative result, recorded:

- `T+0xb8b8` has exactly **two writers**, both inside `FUN_140500200`
  (`0x1405002be`, `0x1405008bd`) - the b8b8 writer the report already documented.
- `FUN_140500200` has exactly **one caller: `0x1404f6e62`**, which sits inside
  `FUN_1404f6c30` - the **team tick** itself, not the manager tick's step 1.
- The instruction immediately before the call is `mov byte [rax+0xb8a2], 0`, an
  unrelated byte (still unresolved, `pes-re-defender-decisions.md` 291). There is
  no ref/level-change guard on the path.
- `FUN_140606cd0` then reads `T+0xb8b8` **inside its 11-slot loop** (OBS mask.c:235).

So PES recalculates the slot mask every team tick and reads it per slot. It is
neither cached across ticks nor hoisted out of the loop, and the report's
"rebuild only when the reference player or the attack level changes" describes
the caps and priorities (`FUN_140600e40`, manager step 1), not this mask.

The port already computes `AttackSlotMask` once per tick and passes it into
`ChanceMaskReject`, which is at least as cheap as PES's structure and semantically
identical. **The histogram's slot-mask share (~45% of the surviving walks) is real
work PES also does, not redundancy.** No hoist was made.

### 9.2 `FUN_140608ac0`, the pass-to-me lock: decoded, three parts wired, the fourth blocked

`FUN_140606cd0`'s real gate order (OBS mask.c:216-235) puts four corridor tests
between `FUN_140593430` and the `T+0xb8b8` mask: `608570`, `6087a0`, `609350`,
`608ac0`. The port omits all four. `608ac0` (raw `0x140608ac0`-`0x140608b33`) is
the first and is fully decompiled:

```
if (event kind != 1) return 0;                    # a pass
if (FUN_140a62ea0(ctx, event target) != id) return 0;   # to THIS slot
if (MI+0x1198 - event age > int(fps·0.5+0.5)) return 0; # within 0.5 s
if (FUN_140a64030(id, 0x12) < 1) return 0;         # the slot's concept entry
if (that entry == 2) { the concept-0 test }        # FUN_140a41b30
```

The first three are observable in GF (`PassEvent` carries kind, target and
time_ms) and are ported and tested. The fourth is **not**: GF has no concept table
at all, so a slot whose decodable conditions all hold reads `kPassToMeUnknown`
rather than a guessed yes or no. That is why the function is deliberately **not
wired** into `ChanceMaskReject`: the raw uses it to admit a special lane, and
admitting on an unknown would repeat the re-run block's mistake.

Measured (nothing wired yet, so expected): runs 394-469, walks 22,560-20,490,
stop fires 344-404 - unchanged from baseline.

### 9.3 Team ticks vs slot walks, now separate counters

`maskWalks` conflated "how often the team may run" with "how many slots were
examined", because the histogram filed the team head's refusal once per refused
tick. `AddTeamTick` now counts the head's invocations and the report prints both,
so a future histogram can be read as (ticks the team may run) x (slots walked)
rather than one mixed number.

### 9.4 Verification

- `pes_offball_test` 42/42.
- Mutation-checked on every decodable term of `608ac0`: the pass-target check, the
  0.5 s age window and the event-kind check.
- The three unmodelled terms (instruction 6, the holder flags, `FUN_1405a5240`)
  are named in the header, not silently dropped.

## 10. The admission chain, fully resolved (commit `13230dc`)

### 10.1 The decisive measurement

With the team head's ticks counted apart from the slot walks:

```
[offball] chance runs 394-469 | team ticks 16485-16171 | mask walks 22560-20490 | stop fires 344-404
```

- The head is asked **32,656** times and refuses **~69%** - PES's own rule, working
  as designed, nothing to fix and nothing to hoist.
- **22,560 walks over 16,485 ticks = 1.37 slots walked per tick where the team may
  run.** Not eleven. **There is no walk flood.** The earlier flood reading came from
  the combined counter, which could not distinguish a refused team tick from a
  slot being examined.

### 10.2 Where the ~600 runs per team actually come from

Of the 42,050 walks where the head passed, **3,747 survive - 8.9%**. The full
histogram, normalised over all 64,535 recorded decisions:

| gate | rejections | share |
|---|---|---|
| team-may-run | 21,485 | 33.3% |
| slot-mask | 10,837 | 16.8% |
| carrier/receiver | 8,230 | 12.8% |
| rank/state | 6,915 | 10.7% |
| reach | 3,900 | 6.0% |
| **pass | **3,747** | **5.8%** |
| role-class | 3,080 | 4.8% |
| generic | 2,670 | 4.1% |
| rank-angle | 1,911 | 3.0% |
| on-the-line | 1,107 | 1.7% |
| box-zone / goal-area / depth / dz | 653 | 1.0% |

With a Chance cap of 3, 394-469 runs over 1200 s implies a run lasts **8-9 s**, and
the stop cascade fires 344-404 times - roughly one release per run started. So a
slot is admitted, runs ~8-9 s, is released, and is re-admitted: the run count is
`survivors per second`, and survivors arrive at ~3.1/s per team.

The four biggest gates are now all **verified faithful** (team-may-run and
carrier/receiver this round, slot-mask and rank/state in 8.4/9.1). None of them is
reducing the count because none of them is wrong.

### 10.3 What is left, and why I stopped here

PES's live ~93 runs per team per 20 min is ~0.0775 runs/s. We produce ~0.33/s -
**4.2x**. The gap is not in the gates: at the measured 8.9% survival and 1.37 slots
walked per runnable tick, matching PES's rate would need either the survival rate
4.2x lower or runs ~4.2x longer.

**The one quantity that would decide which is PES's run lifetime**, and the report
does not record it: `FUN_140606cd0` is called ~32k times per 20 min per team, and
PES's own run count is ~93, so PES's implied run lifetime is ~3·1200/93 = **39 s**
if its cap is also 3. Ours is 8-9 s. That is a **4.7x difference in run duration**,
which is exactly the gap the gates cannot explain.

That points at the **stop cascade** (`FUN_140688c00`), where the port still has
named stand-ins: the stop-test leaves reading PES action records (`5a7fe0`/`5a7be0`/
`5a7d10`), which the assignment itself lists as blocked on a live poke. Until those
are decoded, PES's longer run lifetime cannot be reproduced, and no gate I add will
close a 4.2x duration gap.

**I have not ported anything further this round, and that is the honest result:**
the remaining gap is in a place I cannot reach from the exe alone, and I would
rather name it than fill it with another unfaithful gate.

### 10.4 Verification

- `pes_offball_test` 42/42.
- Full gate **671/671 passed**, standing skips 59 and 154.
- `check_pii.sh` clean, staged by name, pushed:
  `chance-lanes-corridor` = `d8c6673`, `chance-lanes-ticks` = `13230dc`.
- Main's checkout untouched throughout.
