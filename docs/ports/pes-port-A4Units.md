# A4Units: carrier walk rules 7, 8 and 11; Long and Cross units; CurveShot output

SHA `4cca875` (`ls-remote` confirmed). Branch `A4Units`, worktree `<workdrive>/gf-A4Units`, from `origin/main` `0ee3d7a`. No merges; peers A3States/A5Tail confirmed no file overlap.

Walk rules (all red-first, in `pescarrier.*`):
- Rule 7: clear → first of 3/4/9/6 answering, ordered chain per raw `carunits5.c:330`; forced-clear flag keeps the clear; Safety-to-keeper proviso (owned or keeper within 90°). Caught and fixed two of my own wrong expectations against the raw along the way (chain skips but 6's own slot still answers; keeper-far leaves the clear standing, not -1).
- Rule 8: Safety dies in close band, Forward dies on instant return, dribble answers only from kind 16 (class filters `0x14063C9C0`/`0x14063C9A0` decompiled via bridge this session: pass-class = kinds 2–13+15+20–21, dribble-class = 16 only). Corridor/lane battery stays out (lane helpers open) — named in code.
- Rule 11: `UnitArmed` — team caps 0x1E/0x1C/0x1D for 17/3/6 + 5 m ball-distance gate (`dist² ≤ 25`, `0x14259BE20`). Touch windows/turn veto/restart-taker veto are `ponytail:` (state GF doesn't carry).
- Plan-record flags renamed to what they are (`wideSideLatch` +0xD8, `stopAfterFirst` +0xD9, `oneTwoTarget` FUN_14211CB00, `wideSector` imageA+0xB44, `pastCrossLine` plan-5 test) with tests per flag; `FUN_14211CB00` decompiled this session (plan+0xA0==10 → plan id, else slot table — confirms the slot-10 read).

Units (§10 order: Long, CurveShot, Cross):
- `PassLongUnit` (0x14212AD40, re-verified raw): owned-or-danger-low gate, plan-target path, picker 0x142128A20 (advance window (-10,10) + 5 cm height rule — raw-corrected from my first "most advanced" guess), short at openness ≥ 4 (3 with `0x14063A3A0`). Validity helper 0x1421240B0 scanned (328 lines, too deep for this slice) → `planTargetValid` input.
- `CrossUnit` (0x142129590, raw read): 130° carry gate, lane, 16 m unowned lead, tallest-mate picker 0x142125AB0 (raw read, last-max-wins).
- CurveShot output: plan 3 admitted in GF but produced nothing → kind 3 slot now filled from the cross answer (cut-back shape per BackSpace raw); covered by `FillUnitTable` test.
- Cross style: `CrossSpecialist` folded into the Cross plan's skill term; stale `pescarrier.cpp:644` comment made true.
- Controller: Long/Cross answers computed on rethink, table via `FillUnitTable`, dispatch extended (9→longPass, 7/3→crossPass). Plans 5–9: table-fill tests prove Long (9) and Cross (7) adopted with no PassForward in the list.

Tests: `pes_carrier_test` 88/88 with `PES21_ROOT` (87 + install relation, 0 failed), 87/86+skip without. Mutations run, rebuilt, caught, reverted (`git diff` clean after each): M1 chain order→`AClearFallsBack…`, M2 Safety veto→`ASafetyAnswerDies…`, M3 height rule→`WithinTenMetres…`, M4 cross gate→`ACenteringBallNeeds…`, M5 5 m gate→`APassKindBeyond…`, M6 kind-9 fill→`PlanSix…`, M7 cut-back fill→new `CurveShotFills…` test (written because M7 was uncaught). Full build clean. Also ran: `pes_image_test` 16, `pes_tick_a1_test` 12, `sim_match_test` 10 (with install) — all pass. Gate not run (Main gates).

Smoke (2-min seed 7, `PES21_ROOT` set, no lock): off `a3d5e43a3b7b03af` — unchanged from the gate. On `0c6e68e6703f0b79` twice (deterministic), full time, no hang: passes 12–15, shots 1–1. Tripwire passes.

Open: BackSpace/Safety/GoalFront/OneTwo/SideChange/PassTarget/DirectHighBall/Special/Feint units; rule 8 lane battery; rule 7 forced-clear opponent count (flag input exists, always false on-path); rule 3 inert (tier gate off); plan target + validity + team-mode-3/human-carrier flags still defaults; `0x1421240B0`/`0x142128A20` full decodes.