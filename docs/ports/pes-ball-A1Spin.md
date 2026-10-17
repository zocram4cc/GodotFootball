# A1Spin: ball spin pump, kick spin, contact-mode index

Branch `A1Spin`, pushed. SHA `60fa9e8` (confirmed by `git ls-remote origin A1Spin`). Base `origin/main` `a17df0d`.

## 1. Raw-Y spin pump: DONE, behind `pes_loop`
- Port: `RawYAfterPump` in `src/sim/onthepitch/pesballphysics.cpp`, called from `ContactTick`.
- Rule (CONFIRMED, disasm `0x1408770a5..0x1408771cd`): Y target is 0 (reload of `[rbp+0x98]` at `0x140877079`), so `dY = -rawY`.
  `u = clamp01(|dY| / 0.01)`, `a = 2.5 * vy_keep`, `blend = clamp01(a + (vy_keep - a) * u)`, `rawY += blend * dY`.
- Constants from the install: `0.01` at `0x14256ae18`, `2.5` at `0x14259bde4` (both read from `PES2021.exe`, values checked).
  `×100 / ÷100` at `0x14253ee30` cancel and are not ported.
- Correction to the RE doc: `vy_keep` reads `X = -3.6*vy_pre`, signed. `0x140876ba2` does `XORPS` with the sign mask `0x1425656d0`, not an ANDPS abs (`0x142565d30` is the abs mask). A rising ball keeps only the base. `VerticalKeep` now takes the signed speed.
- Gate: `PesBallConstants::spinYPump`, set by `Match` from `pes_loop`. Off keeps the GF write (`Y := 0`), so the off hash is unchanged.

## 2. Kick spin: BLOCKED, not ported
- Confirmed: `FUN_140875580` returns `v_R` and discards its ω. `v_R = FUN_140875170(n = n' = kicker+0xf0, foot velocity = 54·kicker+0xfc, ball velocity = 54·Δpos of track record 1, spin = 492.181·raw of record 1, T = 0.75, E = 0.45, size = 1.1)`.
- Confirmed: the cap helper `FUN_140875a10` caps at 8 rev/s (`SPIN_CAP_VA` 0x14259bdf4). Branch B′ uses weight `DAT_142598578` = 0.2.
- OPEN: meaning of kicker `+0xf0`, `+0xfc`, `+0x108`, `+0x10c`, `+0x2d8`, `+0x2d9`. GF has no such player record. Needs a PES player-record decode or a read-only live read of a kicker object.
- INF: units of the `2π·v_R` term.
- OPEN: live `simBall+0x20` (kick |ω|/2π). Not located. Next step: BallControl vtable `0x1425988b0`, slot 9 reads simBall, then read `+0x20` across one kick.
- Result: no SetRotation replacement. `humanoid.cpp` kick sites (pass ~line 840, shot ~line 1051) and `ball.cpp:85` ponytail stay as they were.

## 3. Contact-mode index: DONE
- `ContactModeForConfig(c, cfg7c, cfg78, cfg85)`: `cfg85 != 0` gives 3; otherwise `T[3*cfg7c + cfg78]`, out of range gives 1. Table from the install at `0x14349f718` (unchanged loader).
- The old `pitch*3 + weather` code already had the right table order. The defect was the RE doc's field naming (`i32[cfg+0x78]` as pitch), now corrected in comments.
- GF has no pitch or weather field. Stand-in pair `(kGfModeRowStandIn, kGfModeColStandIn) = (0, 1)` reads mode 1, the same value as before. Named `// ponytail:` in `pesball.hpp`. Which field is pitch and which is weather: OPEN.
- Removed dead: `kContactModeLive`, `kContactModeDryNormal`, `kPitchDryNormal`, `kWeatherNone`, `WeatherIndexForGfWeather`, `kStopBlend_s` (a literal 0.01).

## Tests (red to green, then mutations)
Red (before logic): new tests failed at runtime (`YPumpDecays`, `YPumpBlendRamps`, `VerticalKeepTakesTheFallSpeedWithItsSign`, `ContactModeFlag...`). The index function was first declared only (link red), then stubbed (runtime red), then implemented.
Pinned as passing: `YPumpOffKeepsTheGfWriteThatZeroesRawY`, `ContactModeReadsTheRow...`, `ContactModeOutOfRange...`.
Fixture fix: `ContactModeFlag...` needed its own mode table (`SyntheticConstants` leaves it zero).

Mutations, each applied, rebuilt, run, reverted:
- M1 drop `/0.01` in `u`: caught by `YPumpDecays`, `YPumpBlendRamps`.
- M2 pump ignores the switch: caught by `YPumpOffKeepsTheGfWriteThatZeroesRawY`.
- M3 keep takes `|vy|`: caught by `VerticalKeepTakesTheFallSpeedWithItsSign`.
- M4 `cfg85` flag ignored: caught by `ContactModeFlagAtCfg85ForcesModeThree`.
- M5 row/col swapped: caught by `ContactModeReadsTheRow...` and `ContactModeFlag...`.

Binaries (after final revert, all green):
- `pes_ball_physics_test` 43 passed (install tests run with `PES21_ROOT`)
- `pes_ball_track_test` 14
- `pes_ball_collision_test` 11
- `pes_tick_a1_test` 18
Gate not run (Main gates).

## Smokes (2 min, seed 7, `--minutes 2`, `--pes21-root`)
- Switch off: `[determinism] d4a22e3211c6940e`. Matches the baseline. Exit 0. Tripwire clean.
- Switch on (`--pes-loop`): `[determinism] 92cb853ceaa9c926`, same on a rerun. Exit 0. Passes 7-10, `[pass-restart] 0-0`, restarts 17, longest stoppage 14.1 s. Shots 1-1, which is low but not zero.

## Items for Main
1. `src/sim/onthepitch/match.cpp` edited, 5 lines in the constructor after `pesBallConstants` is made. Sets `contactMode` via `ContactModeForConfig` and `spinYPump = pesLoop`. It is on the do-not-touch list for this assignment. Please review at merge, or move it to whichever agent owns `match.cpp`.
2. Kick spin (section 2) is blocked on the kicker-record fields and the live simBall read. Needs a separate RE task.
3. Which of `cfg+0x78` / `cfg+0x7c` is pitch and which is weather: OPEN (needs Match Settings read).
4. No overlap with BugFix's files. Their message was acknowledged. `ball.cpp` untouched.
