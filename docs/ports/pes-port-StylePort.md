# PES playing-style gate (R1) and its first pes_loop consumer (StylePort)

Branch `StylePort` (pushed, not merged), commit `20c51c7`, base `origin/main` `9892f50`.
Scope delivered: step 1 (gate, partly) and one step-2 consumer. Most step-2 effects are NOT ported: their PES hosts are not in GF yet (see Open). Nothing was silently shrunk; each item is listed below.

## Decoded

- **Gate** `FUN_1408CB0B0` (CONFIRMED, census R1, `14063E890`/`140A63FE0`/`140A64030`/`141465180` callers): style bit in the player's style word AND `0x142613D20[index]`.
- **Compat rows** (CONFIRMED, read from `pes_full.bin` and asserted by the install test): 10 u32 rows, rows 10-13 zero.
  - row0 GK `{19,20}`; row1 CB `{12,14,15}`; row2 SB `{16,17,18}`; row3 DMF `{10,11,12,13}`; row4 CMF `{8,9,10,12,13}`; row5 SMF `{4,6,7,9,10}`; row6 AMF `{1,4,8,9,10}`; row7 SS `{0,1,4,8}`; row8 WF `{4,5,6,7}`; row9 CF `{0,1,2,3}`.
- **Index = PES role class** (INF, high confidence): FUN_1408c51f0's GK0 CB1 SB2 DMF3 CMF4 SMF5 AMF6 SS7 WF8 CF9. The ten rows line up one-to-one with those classes. GF's `PlayerFacts::position` (13 registered positions) is not the index: under it row 9 would gate LWF and rows 10-12 would gate out RWF, SS and CF wholesale.
- **Engine bits** (CONFIRMED, pes-re-playing-styles.md §0): 0 Goal Poacher, 1 Dummy Runner, 2 Fox, 3 Target Man, 4 Creative, 5 Prolific Winger, 6 Roaming Flank, 7 Cross Specialist, 8 Classic No.10, 9 Hole, 10 Box-to-Box, 11 Anchor, 12 Destroyer, 13 Orchestrator, 14 Build Up, 15 Extra Frontman, 16 Offensive FB, 17 Defensive FB, 18 FB Finisher, 19 Offensive GK, 20 Defensive GK.
- **Style source**: GF reads the style from profile XML (`playerdata.cpp:91`, `playing_style`), not from the save. The save decode (Player.bin field 0x40, EDIT enum through the 22 dwords at `0x142979080` and the intermediate-to-bit switch) is NOT ported. OPEN.
- **The off path runs the run manager too.** `Team::Process` calls `ProcessOffBall`, which calls `ProcessRunManager`, with no `pes_loop` guard (`teamAIcontroller.cpp:272`, `:2202-2219`). Any change there moves the off-path hash unless it is switched on `pes_loop`.

## Changed

- `src/sim/onthepitch/pesstyle.{hpp,cpp}` (new): `EngineBit`, `StyleWord`, `Compat`, `LoadCompat` (reads `0x142613D20`; fails if all zero), `Gate`.
- `pesoffball.hpp/.cpp`: `PlayerFacts::styleWord`; `RunWorld::styleCompat` (set only on pes_loop); `OffBallConstants::styleCompat` (loaded). A `StyleLb` helper replaces both `styleLb` stand-ins (`pesoffball.cpp` near `ChanceMaskReject`'s LB test and the reach path):
  - pes_loop: `Gate(compat, roleClass, styleWord, bit 17)`, i.e. Defensive Full-back style on an SB role (LB or RB).
  - off pes_loop: the old stand-in, `position == 2`, unchanged (`ponytail:`, to go when pes_loop is the only path).
- `teamAIcontroller.cpp` `BuildRunWorld`: `styleCompat` copied only when `match->PesLoop()`; `styleWord` filled from `GetPlayingStyle()` for every player (no effect off path).
- Tests: `tests/sim/pes_style_gate_test.cpp` (new, 5 tests, registered in `tests/sim/CMakeLists.txt`); `tests/sim/pes_offball_test.cpp` gains `ALeftBackReachesOnlyWithTheDefensiveFullBackStyleOnPesLoop`.

## Verification

- **Red**: tests written before the module. Build at that point fails: `pesoffball.hpp:35: fatal error: pesstyle.hpp: No such file`. This is a compile red, not a behaviour red; no stub was written.
- **Green**: `pes_style_gate_test` 5/5 (install row test read `0x142613D20` from `PES21_ROOT`, not skipped). `pes_offball_test` 54/54.
- **Mutations** (one at a time; each caught):
  - M1 gate ignores style word: `NeedsTheStyleBit`, LB consumer test.
  - M2 gate ignores compat row: `NeedsTheRoleRowToListTheStyle`.
  - M3 pes-path `StyleLb` falls back to the stand-in: LB consumer test.
  - M4 off-path stand-in LB to RB: LB consumer test.
  - M5 Defensive FB bit 17 to 16 (collides with Offensive FB): `EachStyleHasItsOwnEngineBitAndNoneHasNone`, `NeedsTheStyleBit`, LB consumer test.
- **Not covered**: `LoadCompat`'s all-zero error path (no fixture); the `PesLoop()` guard in `BuildRunWorld` (checked only by the off-path hash).

## Smoke (seed 7, 2 min, `PES21_ROOT=<workdrive>/PES21`, `gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2`)

- **Off path**: exit 0, HOM 0-0 AWA, `[determinism] a3d5e43a3b7b03af`. Matches the recorded baseline, so the off path is unchanged.
- **On path** (`--pes-loop`): exit 0, HOM 0-0 AWA, `[determinism] 011518e07fbeabc4`. Rerun: same hash, so deterministic. Passes 15-14, accuracy 26-28%.
- The 0-0 score on both paths and the absent shot counter mean shots are NOT verified by a counter. The `011518e07fbeabc4` value is the first pes_loop hash for this change; no pes_loop baseline was given to me.

## Open (assignment items not done)

Step 2 effects. Each host below is either not in GF or its consumer is not wired:
- **Goal Poacher**: `140595AB0` run threshold 30 to 15 and close radius d² 121 to 256 (11 to 16 m). Host is the Chance accept test; `pesoffball.cpp:418` says it is not ported. `140662A10` target floor 25 (`14259BE20`); `1406087A0` minus 12 (`14259BE04`, meaning INF). `5a5b20` floor 26 needs style bit 0 plus the state decode (states 0x1d-0x2c, timer ring); not ported.
- **Target Man**: `1405FC500` candidate +30 (slot-class condition OPEN); `1405E6E80` offset minus 6 times `fVar43` (`14259BDEC`).
- **Hole / Box-to-Box +5**: `1405DEF90` L88-92, `1405DF1F0` L72-74. Hole's dt18 arms (key 0x1D +14 at `140612660`, key 0x1F x1.3 at `14060A8D0`) not ported.
- **Extra Frontman +0.2**: `1405FC860` L62-65 (`142598578`).
- **Full-back blend**: `1405DF5A0` L105-121 (base `(0.5a+b)/1.5`, `1.5`; bit 16 `(a+b)/2`, `2.0`; bit 17 `b`). Not ported.
- **Set-piece clamps**: `140680980` L293-303 (bit 7 to `[0, 0.1]`; bits 5/6 absent to `[0, 0.5]`). Not ported.
- **Keepers**: constants already in `peskeeper.cpp` (range 15 to 17, angle 48 to 43, distribution `f*0.25` / `f+1.1` / `f*0.6`). Style flags come from profile XML at `elizacontroller.cpp:543` and `goalie_default.cpp:185`. The bit-gated keeper path is not rewired here.
- **dt18 profiles**: Box-to-Box (key 0x3A, record 0xBC) and Dummy Runner (key 0x21, record 0xBD). Record values OPEN (dt18 bytes not in the exe).
- **Save-side style decode**: OPEN (above).

Step 3 (knob deletion): nothing deleted. Doc §6 is wrong: `GetShotAppetite`, `GetShootingRangeBonus`, `GetFinisherDemand`, `GetCreatorVision`, `GetPassTypeBias`, `GetSpaceRatingWeight` and the four dribble knobs each have live callers on both paths (`elizacontroller.cpp:1153, 1266, 1272, 1276, 1330, 1342, 1462, 1467`; `AIfunctions.cpp:471, 476, 477, 573`). They stay as off-path stand-ins with no PES site on pes_loop. The doc's §6 "0 callers" should be corrected when this report is archived.

Carrier path, not touched (A4Units; ownership taken from the doc, not checked in code): `1405E6E80`, `1405FC500`, `1405FC860`, `140640CC0`, `140641690`, `140608AC0`. Set-piece `140680980` is outside the carrier path.

## For Main

- Re-run `pes_style_gate_test` and `pes_offball_test` after merge (green here).
- Compare the on-path hash `011518e07fbeabc4` with the pes_loop baseline, if one exists.
- Archive with `tools/dev/archive_re.sh`, and fix doc §6 (callers) in the same pass.

## Main correction (17-10-26)

The claim above that every knob in `pes-re-style-effects.md` §6 has live callers is wrong. `grep` over `src`, `tests` and `godot` finds no caller of `PlayingStyles::GetShotAppetite`, `GetShootingRangeBonus`, `GetFinisherDemand`, `GetCreatorVision`, `GetPassTypeBias` or `GetSpaceRatingWeight` outside their own definitions in `src/sim/data/playingstyles.cpp`. The callers counted were the `PlayerSkills::GetShotAppetite` / `GetShootingRangeBonus` namesakes in `playerskills.cpp`, which are different functions. §6 stands: the six are dead code.
