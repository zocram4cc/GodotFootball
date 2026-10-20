# CrossLane: the Cross unit's lane is the real lane scan, not the pressure class

Branch `CrossLane`, base `origin/S1Port` `62e534c`, merged `origin/main` `285bdc2`.
Worktree `<workdrive>/gf-CrossLane`. Commit `b171ef7`
(`git ls-remote origin CrossLane` = `b171ef710bb5d7c409ed9a7660cbdf89c2607dc3`).

Files: `src/sim/onthepitch/peslane.{hpp,cpp}`,
`src/sim/onthepitch/player/controller/elizacontroller.cpp`,
`tests/sim/pes_lane_test.cpp`. Not touched: `pescarrier.*`, `humanoid.cpp`, the
ball, the referee, the sequencer.

## 0. Verdict

The Cross unit (kind 7, `0x142129590`) was starved by a stand-in: `laneOpen`
was `carrierPressureClass >= 2` (`elizacontroller.cpp:2615` at `62e534c`). PES
tests a **lane**: the core `0x14063CD90` scans the opposing side for players in
the lane, and the unit answers only when the scan records nobody.

`pes::lane::ScanLane` was **already** a faithful port of `0x14063CD90`
(`docs/re/pes-re-lane-core.md` §3, the spec of the core's rule: inputs, the
count output, the annulus-and-cone and segment-crossing tests, the constants
with their operand VAs). So the core was reused, not re-ported. What was new is
the Cross-specific **geometry** that feeds it, and the wiring.

Effect, from a 20-minute seed-7 `--pes-loop` probe (temporary, removed):

| | Cross calls | lane open | answers |
|---|---|---|---|
| before (stand-in) | 86 | 6 | 6 |
| after (real lane) | 75 | 74 | 74 |

The stand-in refused the unit in 93% of the rethink calls; the real lane lets it
through in 99%. One call in 75 found the lane blocked.

## 1. The decode

`0x142129590` (asm `asm_142129590.txt`, decompile `dec_0x142129590.c` in
`<workdrive>/gf-CrossLane-scratch`). The lane call is at `0x142129804`.

### 1.1 The aim (0x14212972F..0x142129744)

```
MOVSS XMM0,dword ptr [RAX + 0x14]     ; the attack sign, C[0x14]
MULSS XMM0,dword ptr [RAX + 0x10]     ; times T18+0x10
MOVSS dword ptr [RSP + 0x50],XMM0     ; the aim's x
```

The aim is `(k·T18+0x10, 0)` — a point on the pitch's **centre line**. The
bearing to it (`0x140474C40`, called at `0x14212974A`) is the lane's heading.

`T18+0x10` is the goal-area line, **F[4] = 47.06 m**
(`docs/re/pes-re-fieldinfo.md` §1: F4 = F0 − 5.5 = 52.56 − 5.5 = 47.06).

> **Doc correction.** `docs/re/pes-re-attack-decisions.md` §2.1 labels F[4] as
> "half-width 34.06". That is **F[1]** (the touchline half-width). F[4] is
> **47.06**, the goal-area line. The Cross aim is `k·F[4]` = the goal-area line,
> not the touchline. The same doc's §7 summary ("F[4] aim, 5×30 lane") is
> right about the field and the lane; only the parenthetical value is wrong.

### 1.2 The origin (0x14212974A..0x14212975B)

```
MOVAPS XMM2,XMM6                     ; r = 1.0 (0x14256AE1C)
LEA RDX,[RSP + 0x60]                 ; the bearing out
LEA RCX,[RBP + -0x80]                 ; the ball point
CALL 0x140a8f7c0                     ; the point 1 m along the bearing
```

`0x140A8F7C0` advances a point `r` metres along a bearing:
`x += r·sin θ`, `z += r·cos θ`. So the **origin is the ball point advanced one
metre along the aim bearing**, and the heading is that bearing.

### 1.3 The lane struct (0x142129779..0x1421297CC)

The struct the core takes is `[x, y, z, θ, len, cone, rad]`, built on the stack
at `RBP-0x70`:

| field | value | VA |
|---|---|---|
| origin | the advanced ball point | — |
| heading θ | the bearing to the aim | — |
| len | 5.0 | `0x14259222C` |
| cone | 30.0 | `0x142591140` |
| rad | 2.0 | `0x142565604` |

The last three are the install constants (`MOVSS XMM0,dword ptr [0x14259222C]`,
`[0x142565604]`, `[0x142591140]`), read verbatim by `readva.py`:
`0x14259222c = 5.0`, `0x142565604 = 2.0`, `0x142591140 = 30.0`,
`0x14256ae1c = 1.0`.

### 1.4 The call and the verdict (0x1421297D7..0x142129814)

```
MOV  dword ptr [RSP + 0x40],0xffffffff   ; out[0]: the nearest slot
MOV  dword ptr [RSP + 0x30],0x0           ; out[1]: the count
LEA  RAX,[RBP + -0x70]                    ; the lane struct
MOV  R8D,ESI                              ; the horizon h = 0
MOV  byte ptr [RSP + 0x28],0x0            ; keep = 0 (the keeper is excluded)
MOV  RDX,R14                              ; the reference slot
MOV  qword ptr [RSP + 0x20],RAX           ; the lane struct
CALL 0x14063cd90                          ; the core
...
CALL 0x1421b7d40                          ; out[1] (the count)
TEST EAX,EAX
JNZ  0x142129baf                         ; the count != 0 -> no answer
```

`0x1421B7D40` is `MOV EAX,[RCX]; RET` — it reads the count. The unit **answers
only when the count is 0**: the lane is free. The scanned side is the one
opposite the carrier's (`0x1421297CC`: `CMP EDI,1 / JA / TEST EDI,EDI / SETZ`,
the `uVar6 == 0` flag), and the carrier's own slot is the reference.

### 1.5 The gate around it

The call sits behind three gates that GF already carries or already stubs:

1. `FUN_140a4c930` (the ball-in-play test) must pass.
2. `ctx4+0x71` (the ball owned / settled) must be non-zero.
3. The carry bearing: `|wrap180(carry − facing)| ≤ 130` (`0x1425A9BF0`).

Gate 3 is still a GF stand-in (`crossIn.carryVsFacing_deg = 0.0f`,
`// ponytail:` the ball→carrier-predicted `0x140474C40` is not ported). It is
**not** the gate this task covers, and it does not block: with the carry gate
passing trivially, the unit answered 74 of 75 calls.

### 1.6 What the cross lane is not

- The **G4 corridor** branch (`docs/re/pes-re-team-mode.md` §7.2d) also calls
  `0x14063CD90`, but with its own geometry (width 5, length 100, cone 90) — a
  different lane, not the Cross unit's.
- `FUN_14063D2F0` is the thin argument packer around the core (it forwards the
  struct and the flags). It is not a second rule.

## 2. The port

### 2.1 `peslane.hpp` / `peslane.cpp`

`LaneConstants` gains the four Cross scalars, read from the install by VA in
`LoadLaneConstants` (the same `LANE_SCALARS` table the core's own constants
use):

```cpp
// --- the Cross unit's lane (0x142129590's call at 0x142129804) ---
float crossAdvance_m;  // 1.0 (0x14256AE1C): the ball point's advance along the aim bearing
float crossLen_m;      // 5.0 (0x14259222C): the lane's length
float crossCone_deg;   // 30.0 (0x142591140): the lane's cone
float crossRad_m;      // 2.0 (0x142565604): the radius offset of the cone apex behind the origin
```

The builder, one function, no new abstraction:

```cpp
LaneRequest CrossLane(const LaneConstants& k, offball::Vec2f ball, float aimX_m) {
  const offball::Vec2f aim{aimX_m, 0.0f};
  const float heading_deg = Bearing(k, ball, aim);
  const offball::Vec2f dir = Dir(k, heading_deg);
  const offball::Vec2f origin{ball.x + k.crossAdvance_m * dir.x, ball.z + k.crossAdvance_m * dir.z};
  return {origin, heading_deg, k.crossLen_m, k.crossCone_deg, k.crossRad_m};
}
```

`Bearing` and `Dir` are the file's existing private helpers: `Bearing` is
`0x140439EF0` (atan2(dx, dz) in degrees, the lane's z = the GF y coordinate),
`Dir` is `dir(θ) = (sin θ, −cos θ)`, which is the file's spelling of PES's
`dir(θ) = (sin θ, cos θ)` over PES's (x, z) with **PES z = −GF y**. The
advance therefore matches `0x140A8F7C0` exactly.

The attack sign enters through `aimX_m` (`attackSign * pesGoalAreaLineX`), so
the builder has no knowledge of the sign's meaning.

### 2.2 The controller (`elizacontroller.cpp`)

The Cross block **moves** from its old place (before the GoalFront block) into
the lane-units block, next to BackSpace, Safety, OneTwo and DirectHighBall.
That move is required, not cosmetic: the lane needs the **22-slot lane space**
(`bs.players`) and the **ctx4 ball point** (`ctxPoint`), and both are built in
that block. The old place had neither.

The stand-in is **deleted**, not wrapped:

```cpp
// before (62e534c)
crossIn.laneOpen = carrierPressureClass >= 2;  // ponytail: 0x14063CD90 is not ported

// after (b171ef7)
const pes::lane::LaneRequest crossLane = pes::lane::CrossLane(
    carrier.lane, {ctxPoint.x, ctxPoint.z}, attackSign * pesGoalAreaLineX);
crossIn.laneOpen = pes::lane::LaneFree(carrier.lane, bs.players, bs.carrierSlot, crossLane,
                                        /*keepKeeper*/ false, /*horizonFrames*/ 0);
```

`LaneFree` is the existing `0x14063DC10` port: it scans the side opposite the
reference and is true when the count is 0. The flags match the asm: keep = 0
(the keeper is excluded) and h = 0 (no prediction horizon).

The `pesGoalAreaLineX` constant is GF's existing F[4] (47.0599976,
`src/sim/gametypes.hpp:83`), already used by the defensive shape and the
offside rule.

## 3. The tests

`tests/sim/pes_lane_test.cpp`, three new tests on the `LaneCoreTest` fixture
(the fixture reads the constants from the install, so the tests are skipped
without `PES21_ROOT`):

- `CrossLaneGeometryComesFromTheBallAndTheGoalAreaLine` — the ball on the
  centre line attacking +x: the heading is 90, the origin is one metre along it,
  and the lane is 5 m / 30° / 2 m.
- `CrossLaneHeadingPointsAtTheAimAndAdvancesOneMetre` — the ball at (−40, 0)
  attacking −x: the heading is 270 and the origin is at −41.
- `CrossLaneScansTheOpponentsAndCountsThem` — **the value test**: a concrete
  ball and a concrete opponent, and the count. One opponent on the lane gives
  `count == 1` and `nearest == 11`; with the lane clear the count is 0 and
  `LaneFree` is true.

### 3.1 Red → green

The API had to exist for the test to link, so the red was taken with the API in
place and the **geometry** still a stand-in (the A4Lanes precedent: stub the
behaviour, not the build). The stand-in ran the lane from the ball along a fixed
90°.

RED, `PES21_ROOT=<workdrive>/PES21 ./build/tests/sim/pes_lane_test --gtest_filter='*Cross*'`:

```
[ RUN      ] LaneCoreTest.CrossLaneGeometryComesFromTheBallAndTheGoalAreaLine
pes_lane_test.cpp:248: Failure
Expected equality of these values:
  l.origin.x
    Which is: 0
  1.0f
[  FAILED  ] LaneCoreTest.CrossLaneGeometryComesFromTheBallAndTheGoalAreaLine (0 ms)
[ RUN      ] LaneCoreTest.CrossLaneHeadingPointsAtTheAimAndAdvancesOneMetre
pes_lane_test.cpp:259: Failure
Expected equality of these values:
  l.heading_deg
    Which is: 90
  270.0f
pes_lane_test.cpp:260: Failure
Expected equality of these values:
  l.origin.x
    Which is: -40
  -41.0f
[  FAILED  ] LaneCoreTest.CrossLaneHeadingPointsAtTheAimAndAdvancesOneMetre (0 ms)
[ RUN      ] LaneCoreTest.CrossLaneScansTheOpponentsAndCountsThem
pes_lane_test.cpp:276: Failure
Expected equality of these values:
  s.count
    Which is: 0
  1
pes_lane_test.cpp:277: Failure
Expected equality of these values:
  s.nearest
    Which is: 255
  11
pes_lane_test.cpp:278: Failure
Value of: lane::LaneFree(k_, blocked, 3, l, false, 0)
  Actual: true
Expected: false
[  FAILED  ] LaneCoreTest.CrossLaneScansTheOpponentsAndCountsThem (0 ms)
[  PASSED  ] 3 tests.
[  FAILED  ] 3 tests, listed below:
[  FAILED  ] LaneCoreTest.CrossLaneGeometryComesFromTheBallAndTheGoalAreaLine
[  FAILED  ] LaneCoreTest.CrossLaneHeadingPointsAtTheAimAndAdvancesOneMetre
[  FAILED  ] LaneCoreTest.CrossLaneScansTheOpponentsAndCountsThem
 3 FAILED TESTS
```

Three behavioural failures for the right reason (the geometry, not a typo).

GREEN after the real implementation:

```
[==========] 24 tests from 2 test suites ran. (1 ms total)
[  PASSED  ] 24 tests.
```

`pes_carrier_test`: `[  PASSED  ] 186 tests.`

### 3.2 Mutations

Three, each caught, each reverted:

| mutation | caught by | failure |
|---|---|---|
| the advance 1.0 m → 2.0 m | `CrossLaneGeometryComesFromTheBallAndTheGoalAreaLine`, `CrossLaneHeadingPointsAtTheAimAndAdvancesOneMetre` | `l.origin.x` −42 vs −41 |
| the radius offset 2.0 → 0.0 | `CrossLaneGeometryComesFromTheBallAndTheGoalAreaLine` | `l.rad_m` 0 vs 2 |
| the lane length 5.0 → 2.0 | `CrossLaneGeometryComesFromTheBallAndTheGoalAreaLine`, `CrossLaneScansTheOpponentsAndCountsThem` | `s.count` 0 vs 1 |

The length mutation is the one the count test catches on its own, which is the
point of pinning the count.

## 4. The probe (temporary, removed)

A temporary `std::fprintf(stderr, "[gfprobe] cross laneOpen=%d answer=%d\n", …)`
at the end of the Cross block. The "before" tree was a throwaway worktree at the
base `62e534c` with the same probe; it has been removed
(`git worktree remove --force <workdrive>/gf-CrossLane-before`, after unlinking
its `extern/godot-cpp` symlink).

**The probe only fires under `--pes-loop`.** The whole PES carrier path is
gated on `match->PesLoop()` (`elizacontroller.cpp:2519`), so a probe run
without the flag gives zero hits. Worth recording for the next probe.

20-minute seed-7 `--pes-loop` runs:

| | calls | `laneOpen=1` | `laneOpen=0` | answers |
|---|---|---|---|---|
| before (`62e534c`, the stand-in) | 86 | 6 | 80 | 6 |
| after (`b171ef7`, the real lane) | 75 | 74 | 1 | 74 |

The before run's hash is `4ea0c2860ca4834a` — Main's reference 20-minute
pes-loop hash, confirming the before tree was clean.

Match summary, before → after: passes 10-13 → 17-15, accuracy 30%-69% →
47%-46%, clean 20%-61% → 41%-33%, shots 2-3 → 3-2, xg 0.01-0.02 → 0.34-0.02,
crossings-no-shot 0-2 → 12-0.

The probe was removed before the commit: `grep gfprobe` in the source = 0, and
the `<cstdio>` include it needed was dropped with it.

## 5. Smokes (2-minute, seed 7, clean tree after the merge)

- **Switch-off** (no `--pes-loop`): exit 0, `[gfsim] Full match complete: HOM 0 - 1 AWA`,
  `[ticks] 24090 sim ticks to full time (cap 194400)`, `[idle] 19 episodes, stalls 12`,
  **`[determinism] 75a11cdb2b3c8ec5`** — the switch-off hash, **unchanged** from
  the `62e534c` base (the cross lane is on the PES path only).
- **`--pes-loop`**: exit 0, `[gfsim] Full match complete: HOM 0 - 0 AWA`,
  **`[determinism] c7aea7fc660babd7`**, `[ticks] 39564 sim ticks to full time
  (cap 194400)`, `[idle] 27 episodes, stalls 19`.

**The two hashes behave differently, and that is the point.** The switch-off
hash is **unchanged** (`75a11cdb2b3c8ec5`, the same as the `62e534c` base): the
Cross unit lives on the PES path only, so with the switch off nothing moves.
The `--pes-loop` hash **does** change, from the base's `3d92d259fe45c5dd` to
`c7aea7fc660babd7` — the Cross lane now feeds the pass layer, so the match
plays differently. That is the intended effect of the port, not a regression.
Main's 20-minute pes-loop reference `4ea0c2860ca4834a` is the **before** run
(§4); the after 20-minute hash is `1db413175d67e810`.

## 6. What is still open

1. **The Cross carry gate** (the 130° test, `0x1425A9BF0`) is still a stand-in:
   `crossIn.carryVsFacing_deg = 0.0f`, with the `// ponytail:` marker naming the
   ball→carrier-predicted `0x140474C40` as the PES site that removes it. The
   unit answers 74 of 75 calls with it passing trivially, so it is the next
   gate that will starve the unit once it is ported.
2. **The receiver lead** (`FUN_140a4ee40` and the 16 m test, `0x14259BE10`) is
   unported: `CrossInput::receiverLead_m` is not set, and `CrossUnit` skips the
   lead test when the ball is owned.
3. **The near-duel scan** (the `+0x12A7` byte, the 100.0 quality filter) and the
   f11 / f12 flags are not ported. They set flags on the answer, not the answer
   itself.
4. `docs/re/pes-re-attack-decisions.md` §2.1 still labels F[4] as "half-width
   34.06"; it is 47.06 (see §1.1). The doc was not edited by this branch.

## 7. Files changed

```
 src/sim/onthepitch/peslane.cpp                        | 16 ++++++++
 src/sim/onthepitch/peslane.hpp                        | 13 ++++++
 src/sim/onthepitch/player/controller/elizacontroller.cpp | 50 ++++++++++++----------
 tests/sim/pes_lane_test.cpp                           | 42 ++++++++++++++++++++++
 4 files changed, 100 insertions(+), 21 deletions(-)
```

The controller diff is 29 insertions and 21 deletions because the Cross block
moves; the stand-in line is deleted outright, and no shim, alias or fallback
is left behind.
