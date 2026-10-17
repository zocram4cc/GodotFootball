# DefensiveShapePort — PES defensive line, length/width and marking ported to GF

**Commits:** `7b0cebe` (the port), `9dfe441` (unit tests + corrected chain) on top of `e18f443`.
**Worktree:** the DefensiveShapePort worktree (detached HEAD).

## Verification actually run

| check | result |
|---|---|
| `cmake --build build -j6` | clean |
| `pes_defensive_shape_test` | **24/24 pass** |
| full gate `ctest -j3 --timeout 2400` | **380/380 pass**, 2 skipped (install-only: VfsInstall, EditSaveInstall), 187 s |
| `tools/check_pii.sh` | pass (exit 0) |
| mutation check (15 mutations of the ported logic) | 12 caught, 3 uncaught (noted below) |
| `git diff --cached --stat | grep extern` | empty |

## Changed GF functions

| GF function | PES rule | VA / evidence |
|---|---|---|
| `pes::LoadDefensiveShapeConstants`, `pes::ShapeFieldFromPitch` (new) | the shape's constants: 25 `.rdata` floats read from `PES2021.exe`, plus the live `basePosition.o` record the report read at runtime (vtable 0x142b5a328) | report §8, §14 |
| `pes::DefensiveLineHeight` (new) | `FUN_1405f9eb0`'s ramp, `FUN_1405f7a00`'s cushion, `FUN_1405ef910`'s step-up, the `min(x,0)` / `max(x,-F[4])` clamps | 0x1405f9eb0, 0x1405f7a00, 0x1405ef910; report §6, §14.1 |
| `TeamAIController::CalculateDefensiveShape` (new, every tick from `Process`) | the chain plus `FUN_1405efbd0`'s midfield catch-up (top-3 descending score) | 0x1405efbd0, 0x1405f80c0; report §14.1 |
| `TeamAIController::GetAdaptedFormationPosition` depth terms | PES's three bands off the one line height (back line on the line, midfield `midGap_m` above, forwards at `line+length`) | report §6, §14.7 |
| `pes::LineHalfWidth_m` + the band code in `GetAdaptedFormationPosition` | `FUN_140688300` + `FUN_14063f230`: 15/19/23 m for 3/4/5 (10 below 3), ±2.85 m by Compactness, −6 m per attack level, min spacing 6/5, 0.85·halfWidth cap, the ball-side squeeze, the lateral shift toward the ball's touchline | 0x140688300, 0x14063f230; report §7 |
| `pes::TeamLength_m` | 46/42/32 m by style and attack level, −0.33 m per Compactness step | report §14.1 |
| `TeamAIController::CalculateManMarking` (rewritten) | `FUN_140534a20`: most-advanced-first order with a 1/6 s projection; passes 0, 1, 2b, 6, 7; the hand-off delay. Every frame from `Team::Process` (was 400 ms); the at-most-three cap deleted | 0x140534a20, 0x140531490, 0x140531260, 0x1405325e0, 0x14052f720, 0x140532f70; report §3, §14.2 |
| `TeamAIController::GetMarkSpot` (new) + `PlayerController::AddDefensiveComponent` | `FUN_140623750`: goal-side point at `6 − 4·ramp(depth)` m, +2 m with Pressuring | 0x140623750; report §14.3 |
| `TeamAIController::IsOffsideTrapArmed` (new) and the gate in `default_def.cpp` / `default_mid.cpp` | the trap is armed only on team flag `T+0xe4`; the `offside_trap` tactic stands in and defaults to off, PES's live value for both teams | 0x14057ba50; report §14.4 |
| `TeamAIController::CalculateMarkingQuality` (deleted) | replaced by the mark spot | report §14.8 |

Plumbing: `pesdefshape.hpp/.cpp` new; `matchsetup.hpp`, `match.hpp/.cpp`,
`gdext/pes_library.*`, `gdext/gf_match.cpp`, `apps/gfsim_run.cpp` and
`tests/sim/pes_match_constants.hpp` carry the required constants struct; `tests/sim/CMakeLists.txt`
adds the test target.

## Slider mapping

| PES input | GF source |
|---|---|
| Defensive Line (k2) | `position_defense_depth_factor` via the live tactic pipeline, `1 + 9·value` |
| Compactness (k3) | ponytail: GF has none; `position_defense_width_factor` read inversely, `11 − 10·value` |
| Defensive Style (byte 4) | ponytail: none; style 0 |
| attack level `T+0xb8a4` | ponytail: none; 2 (PES's live value for both teams) |
| Pressuring (byte 6) | ponytail: the `Aggressive Defence` touchline instruction |
| team level `FUN_140a60340` | ponytail: skipped (PES's live level 2 is worth +5 m of length, so the port is 5 m short) |
| hand-off organiser byte `b` | ponytail: 0, PES's slowest hand-off (~1.1 s) |

## Tests (`tests/sim/pes_defensive_shape_test.cpp`, 24 cases)

Line clamps and slider effects, the style-1 sine offset, no smoothing, length 46/42/32 and the 0.33 m
step, widths 15/19/23/10, the compactness and level narrowing, both squeeze directions, the caps,
mark distance 2–6 m and the +2 Pressuring, the mark spot, the opponent order key, the hand-off delay,
the loader against the install's own floats, and four headless-Match cases for the trap gate, the
mark assignment and the live line.

Mutation check: 15 mutations, 12 caught. The three uncaught (most-advanced sort direction, mark
search radius, hand-off commit condition) are controller-level mutations whose effect is a
distribution over sampled mark pairs.

## Kick-off correction

Consumed by omission: the only two consumers of `FUN_140a74ec0` in the line chain are
`FUN_1405f4f50`'s cover drop and `FUN_1405fb6f0`'s in-possession hold-back, and **neither is in this
port**. Nothing here keys a timer to a change of possession.

## Open

1. **Smoke run numbers not collected.** The 20-minute seed-1000 measurement was attempted twice on
   a scratch sampler; each run takes ~17 min of wall time and the before-run was cut off by its
   shell timeout before printing. **No before/after median line height, team length or `[balance]`
   numbers are in hand.** To finish: run the sampler (`build/apps/smoke_line 1000 20` from the
   worktree root, `PES21_ROOT` set, stderr discarded) on both this tree and a tree at `e18f443`,
   plus `build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 1000
   --minutes 20 --pes21-root ...` for `[balance]`.
2. `FUN_1405f4f50` cover drop (up to 6 m off the deepest cover man): needs a per-slot COVER state GF
   does not have. Largest known gap — PES's live back line sat a median 2.9 m deeper than its own
   line height largely from this pass.
3. `FUN_1405fb6f0` in-possession line and its 20 m kick-off hold-back (5–8 s fade): GF's attacking
   shape kept.
4. Pressing (one MATCH_UP chaser, two covers, second-man reasons): GF's `ApplyTeamPressure` /
   `ApplyZonePressure` stand.
5. Mark passes 3, 4, 5; the steal test, zone limits, mark-spot blend and line-hold cut.
6. The three uncaught mutations want a per-tick mark-count bound and a flips-per-mark bound.

The full report with the AI-map row text is at `pes-re-defensive-shape-port.md`.