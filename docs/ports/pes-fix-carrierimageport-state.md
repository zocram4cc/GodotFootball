# CarrierImagePort — hand-over state at rest (no code written)

Stopped by the Orchestrator before any edit. **No files were changed: `git status` is
clean on `main` (`c21fa11`), no `gf-CarrierImagePort` worktree exists, nothing was
committed, nothing pushed, no branch `refs/heads/CarrierImagePort` exists.**

Everything below is the decode/design verification I completed before the stop, so
the restarted pair does not re-derive it. Sources: `docs/re/pes-re-com-styles.md`
(archived) and the ComStylesRE scratch decompiles at
`<workdrive>/gf-ComStylesRE-scratch/out/` (dec_/asm_/raw_ files, quoted VAs verified
there this session).

## File ownership plan (what the restarted pair must touch)

| File | Change | PES VA |
|---|---|---|
| `src/sim/onthepitch/pescarrier.hpp/.cpp` | `kPlanCurveShot` in `PlanStartProbability`; constants `curveShotComBase/base 30`, `curveShotSkillBase 30`, `curveShotStatGain 10`, `curveShotFloor 5`, `curveShotCap 70`; add per-style truncation order for CurveShot (float add per stat, truncate per stat — see §1) | 0x14211af20 |
| `src/sim/onthepitch/pescarrier.hpp` | `kPlanPriorityOrder[]` add `7` (currently `{9,8,2,3,1,4,5,6}`) | 0x14211E2D0, table 0x142BF02C0 + inline `local_38=7` |
| `src/sim/onthepitch/player/player.hpp/.cpp` | per-player Intention state (BallPlayer+0x32B0, state at +8): field, getter/setter, ratchet update, reset-to-1 | 0x1421197B0, reset 0x14210E5F0 → 0x1415595D0 |
| `src/sim/onthepitch/player/controller/elizacontroller.{hpp,cpp}` | image-state bridge `in.imageState = <player Intention>` (replacing the `= 0` at ~2174), keep `imageState` field name with a `// ponytail:` naming 0x1421197B0 as the site that removes it | 0x1421197B0 |
| `src/sim/onthepitch/pescarrier.hpp/.cpp` | image-manager arbiter state: selected id, frames-since-change, mode, heading | 0x14211E2D0 (+0xA0 selected, +0xA4 frames, +0xC0 heading, +0xC4 mode, +0xCC = vtable[7]) |

`pescarrier.*` IS touched (CurveShot + priority 7 + arbiter) — coordinate with
`CarrierGainFix`, which is editing the same files for the two gain loads.

## 1. CurveShot ImageUnit (0x14211AF20, fully decoded this session)

Verified from `raw_14211af20.txt` / `dec_14211af20.txt`:

- **Admission gates**, in the order the binary takes them:
  1. `FUN_141052e30(a2[0x20])` (the Intention read) `> 2`, i.e. **state ≥ 3** (`cmp eax,3; jl`).
  2. `*(float*)(a2[0x18] + 0xb40) <= 0` — the "+30 is not from the cited VA" note; this is
     a required non-positive float on the trajectory/context object, at **+0xB40**.
  3. `*(char*)(team + 0xb8a2) == 0` — full-attack flag clear.
  4. Side/foot check via `+0x48 == 1` branch (stronger-foot compare; see below).
- **Base:** `-( ((comMask>>4)&1)==0 ? 0 : 0x1e )` → 30 when COM bit 4 (Incisive Run) set,
  else 0; then `+ 0x1e` (30) when `*(byte*)(player+0x40) >> 7 == 0` — skill **7** gives the
  extra 30. So base = 0 / 30 / 30 / **60**.
- **Stats:** KP `byte[+0x2a]` and Finishing `byte[+0x1a]`, each `max(0, min(stat-70, 29))/29`,
  multiplied by **10** (0x142588210) and added to the running integer with `addss` +
  `cvttss2si` — **float accumulate per stat, truncate per stat** (the same shape as
  Cross/Long, not the integer-per-stat shape of Breakthrough/MiddleShot).
- **Floor 5 / cap 70** (`mov eax,5` at 0x14211B106; `cmp ebx,0x46` at 0x14211B0FA).
- **Seed:** `*(float*)(a2[2] + 0x18) * K1300` (0x14260B388), slot 2, range 100; accept if
  `score >= draw` (`cmp ebx,eax; jae`).
- **Final gate when accepted:** 47266 clear, else skill 7 (`0x14063e960(...,7)`), else
  COM 4 (`0x14063e830(...,4)`).
- The side-check constants at 0x142588218 / 0x14256ae1c are the ± signs the
  `+0x14 ^ 0x80000000` branch compares; the pair needs a `strongerFoot` input only if it
  ports that branch — otherwise leave it out (it is not needed for the P-table value, and
  its exact meaning is NOT VERIFIED). The `+0xB40 <= 0` gate likewise needs an input GF
  does not carry: stand in as a caller bool with a `// ponytail:` naming 0x14211AF20's
  `a2[3]+0xB40`.

## 2. Intention member (BallPlayer+0x32B0, state +8) — decoded this session

From `asm_1421197b0.txt` / `dec_1421197b0.txt` (updater) and `dec_1415595d0.txt`:

- Member init/reset writes **1** to `+8` (0x1415595D0, called from reset 0x14210E5F0).
- Ratchet, in the updater's order (transitions only upward; reset returns to 1):
  - `state == 4` → stays 4.
  - else if `IsClose(pred+0x9cc)` (`0x140699820`) and **|headingDiff| < 100°**
    (`COMISS XMM3(=100.0 @0x14253EE30), |diff|; JBE`) → **4**. (Correction already in the
    doc: 100°, not 105°.)
  - else if `state == 3` OR `attackDir*(a4+0x40) > pitch[6]` OR `pred[0xb58]+10 >
    pred[0xb3c]` → **3**.
  - else if `a6+4 < 2 && state != 2` → the two corridor-query blocks; if either overall
    count `> 1` and not (`local_140 > 2 && local_13c != 1` failing the
    `frames*2 <= ctx+0x10` test) → **1**; otherwise → **2** (the latch).
  - else → **2**.
- GF plan: field `int carrierIntention = 1;` on `Player` (PES +0x32B0+8), `SetPesIntention/
  GetPesIntention`, a pure `pes::NextIntention(...)` helper in pescarrier.* taking the
  decoded inputs (closeBand, headingDiffDeg, attackDot, secondQuery flags) so the ratchet
  is unit-testable without a Match, and a `ResetPesIntention()` called from the existing
  reset path with a `// ponytail:` naming 0x14210E5F0.
- GF's non-PES inputs must be named stand-ins (`// ponytail: ...`): pitch[6] → the
  penalty-line mark GF already uses (`pesFieldHalfLength - SetPieceLaws::kAreaDepth`),
  `pred+0x9cc` → the shoot zone's `IsClose(zone)` (already ported), the two corridor
  queries → GF's own corresponding booleans.

## 3. Image gates that must read Intention

- Long Ranger `<3` at 0x14211B4EB (`CMP EAX,3; JGE`) — MiddleShot.
- Speeding Bullet / Incisive `>=3` (0x14211A800 Breakthrough `CMP EAX,3; JL`;
  CurveShot `>=3` as above).
- feint `==3` (ThinkUnitDribble 0x1421306A4).
- Long veto rejects 3/4 (0x14211A570).
- MiddleShot `+40` when state **2** (0x14211B686 `add ebx,0x28`).

## 4. Stat gains — NOT THIS PAIR'S WORK

`CarrierGainFix` (worktree `gf-CarrierGainFix`, on `c21fa11`, already told me its scope)
is landing: assignment of `breakthroughStatGain` (15 @0x14259BE0C) and `crossStatGain`
(20 @0x14259BE1C) in `LoadCarrierConstants`, the dispatcher order fix, "MiddleShot before
Breakthrough", and the per-stat truncation order. It explicitly does NOT touch the
Breakthrough divisor/three-stat fix (F10). **Do not duplicate the gain loads; rebase on
`refs/heads/CarrierGainFix` when it lands and read the gains from it.**

## 5. Arbmance/arbiter design drafted (not implemented)

- `pescarrier.hpp`: `struct CarrierImageSelection { int id = kPlanBasic; int framesSinceChange = 0; int mode = 2; float heading_deg = 0.0f; };`
  (mode init 2 is PES's `+0xC4` init, from reset 0x14210E5F0's `+0x41ac = 2` block.)
- Cancel test **before** admission: the dispatcher 0x14211E2D0 runs `update[1]` on all
  units, then `cancel[3]` on the selected unit (clearing +0xA0 when it returns true),
  then admission `[2]` in priority order `[9,8,2,3,1,4,5,6,7]` only when selected==0,
  then `[4]` on-select, `[5]` active, `[7]` getter → +0xCC.
- GF mapping: a pure `pes::AdvanceImageSelection(CarrierImageSelection&, ...)` in
  pescarrier.* plus per-plan pure cancel/admission predicates so it is testable; the
  ElizaController wires it where `carrierPlan`/`carrierPlanStamp` currently sit
  (`elizacontroller.cpp:2079-2080`).
- Clear-selection condition from 0x14211E2D0: `MI+0x116C == 2 && MI+0x11D4 != 0` — **needs
  Match's period/phase block, which GF does not carry: leave it out with a `// ponytail:`**
  naming `*(MI+0x116C)==2 && *(MI+0x11D4)!=0` (0x14211E2D0 head).

## 6. TDD plan the restarted pair should follow

Per piece: red test in `tests/sim/pes_carrier_test.cpp` (and a new
`tests/sim/pes_image_test.cpp` if the pair prefers a file for the selection arbiter and
the ratchet), run it red, then implement, then a mutation per branch. Mutation checklist
(one edit each, re-run only the affected tests):

1. CurveShot floor 5 → 0 (must fail the floor test).
2. CurveShot cap 70 → 80 (must fail the cap test).
3. CurveShot admission `>=3` → `>3` (must fail the state-gate test).
4. CurveShot base: COM4 term 30 → 20 (must fail the base test).
5. CurveShot per-stat truncation: truncate once at the end instead (must fail the
   per-stat test, e.g. base 60 with two 85 stats).
6. Intention: 100° → 90° threshold; state-3 branch `>` → `>=`; state-1 `>=3` team term
   dropped; init `1` → `0`.
7. Selection: cancel-before-admission swapped (admission first); priority order `7`
   dropped; frames-since-change not incremented.
8. Bridge: elizacontroller passing state 0 again must fail the feint/`>=3` gate test.

## 7. Verification commands for the restarted pair

- Build: `nice -n 19 cmake --build build -j3` (in the new worktree).
- Gate: `flock <workdrive>/gf-sim.lock env PES21_ROOT=<workdrive>/PES21 PES21_EDIT_SAVE="$(find ~/Documents/KONAMI/'eFootball PES 2021 SEASON UPDATE' -name EDIT00000000|head -1)" nice -n 19 ctest --test-dir build -j3 --timeout 2400`.
- Smoke: `flock <workdrive>/gf-sim.lock nice -n 19 build/apps/gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 20 --pes21-root <workdrive>/PES21` (compare against main's
  `[determinism] 9191772f1f1ab525` line from tasks/14-10-26).
- Scratch for this pair: `<workdrive>/gf-CarrierImagePort-scratch`, scripts under
  `ulimit -v 2000000; nice -n 19 ionice -c3`.

## 8. Notes for whoever restarts

- Nothing was pushed; create the worktree fresh:
  `git -C <workdrive>/GodotFootball worktree add --detach <workdrive>/gf-CarrierImagePort origin/main`
  then `rm -rf extern/godot-cpp && ln -s extern/godot-cpp extern/godot-cpp`.
- Main's checkout is clean; no `.git/config` was touched by me.
- Seeds run so far: **none** (no smoke was started).
