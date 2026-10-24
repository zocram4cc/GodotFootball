# PES re-port audit 7: the earliest landed ports not yet covered (PortAudit7RE, 24-10-26)

Read-only audit of the Batch C, Batch B (not covered by audits 1-6), and TurfEV ports on `origin/main`
(`4da64c2`), checked against PES's bytes. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request at a time (decompile only
where the doc did not already verify; constants below re-read from the image by script). GF code read from
`origin/main` via `git show` into `<workdrive>/gf-PortAudit7RE-scratch/` (absolute outputs there). No repo
edits. Budget: ~150 requests; this pass used ~90 (bridge + git-show reads).

Method of `docs/re/pes-re-port-audit.md` (audit 1): every claim gets GF file:line on main, a PES VA with
bytes or shader line / install value, what differs, its effect, the fix. CONFIRMED = bytes/disassembly read
this pass; the cited doc = already verified in the named report; INF = inference; OPEN = blocker named.
Verdicts per port, coverage list, remaining-unaudited list at the end.

Reference docs (all on `origin/main`): `docs/ports/pes-port-RoleArrays.md`, `-TeamSliceA.md`,
`-TeamSliceC.md`, `pes-port-FarChasePressMain.md` (+ `pes-port-FarChasePress.md` superseded note),
`pes-port-Parity.md`, `docs/re/pes-re-press-layer-port.md`, `-press-coord.md` (PressCoordRE),
`-press-eval.md` (PressEvalRE), `-press-life.md` (PressLife), `-far-chase.md`, `-walk-speed.md`,
`-throwin-choreography.md`, `-kicker-standby.md`, `-setpiece-kicker.md`, `docs/ports/pes-goalkick-GoalKickPort.md`,
`pes-port-DodgeGate.md`, `docs/re/pes-re-team-layer2.md` §1-§4, `docs/render/pes-render-PortAudit4.md`,
`docs/render/pes-render-TurfColour.md` §§1-6.

## Verdicts per port

| Port | Verdict |
|---|---|
| MergeRA: RoleArrays (`pesroles.*`) | **PASS with 2 findings** (F1 tail-swap input over-strict vs PES — bounded, no smuggled bug; F2 head-write one-tick lag openly unmeasured — advisory). The class-map loader, bulk writers, swap, and all reader re-basings check out. |
| MergeRA: TeamSliceA (`pesteamtick.*` + wiring) | **PASS with 1 finding** (F3 tail-swap gate extended from `possession` to `possession && latch-ok` — hunk-verified, bounded, fix named). Drift, flags, restart word, stopped select, reaction clear, support weights, counters all check out. |
| MergeRA: TeamSliceC (publisher `pesdesignation.*` + wiring) | **PASS with 2 findings** (F4 `keeperFirst` mis-feeds `RankScore` under keeper-chase — bounded, one-line fix; F5 `DesignateTwoCf` still has no caller — carried OPEN from the port doc). Publisher, orders, fill stages, wiring check out. |
| MergeRA: PrepareSetPiece + DefensiveLine follow-ups | **PASS, no findings** (follow-ups are one-liners inside audited functions; verified present and correctly gated). |
| FarChasePress2 (press coordinator + per-man read + carrier + trap frame + ActionPress) | **PASS with 3 findings** (F6 gate folds `T+0x78`-invalid into `kickValid=false` — same verdict, narrower input, fix named; F7 maintain step 8 (the refusal `FUN_140633350`) not ported — carried OPEN, bounded by the eval doc's own gating; F8 coordinator class inputs 4/1 hardwired (class is always 5-or-7 — marked stand-in, bounded). The dispatch order, holder rule, loose writer, ActionPress target/pace, target tick, carrier-from-touch key push, and trap-frame all check out. |
| Parity (observer, `ParityPort`) | **PASS, no findings** (observer-only, determinism-neutral; F1–F8 from its own report are doc findings, not code divergences). |
| Press layer RE (`pes-re-press-layer-port.md`) | **Not a port** (no code committed by design) — **no findings possible**; the decode it defers is now superseded by PressCoordRE/PressEvalRE/PressLife (audited here through their GF consumers). |
| Batch B items not covered by audit 5 (highest traffic first) | **PASS with 4 findings** (F9 FK/corner/penalty run-to-ball still `FloorVelocity` (audit 5 F3's carrier arm generalized) — bounded, fix named; F10 `GoalKickStandPoint` negates height where PES keeps it — bounded ~1 cm, one-line fix; F11 `UpdatePesDesignation` never refreshes the corner ordinals when eligibility changes mid-stoppage — bounded; F12 `WriteSideRoles` head-write walks `GetAllPlayers()` assuming slot order — matches today, fragile, assert named). Details §8. |
| TurfEV (EV port: does what it ported still hold after GradePort, SunPort, PostPort?) | **PASS with 2 advisories** (T1 `32·a²` alpha term still omitted — directionally safe, carried; T2 full-frame static rect still ships while the live `param_3+0x8c` rect writer is untraced — carried). Law, constants, white-balance bypass, sun state all still hold. TurfFin was never landed (no `FIN_NET` on main) — correctly not ported. |

## Findings (numbered, each with evidence and a proposed fix)

### F1 — `SwapB` refuses pairs PES accepts when the latch is stale (bounded, over-strict input validation)

- GF (`src/sim/onthepitch/pesroles.cpp:86-91` on main): `SwapB` returns early unless **both** `a` and `b`
  are in `0..10`. `TailSwapAllowed` (`:82-84`) reads the latched pair.
- PES: `FUN_140A422C0` is the B half; the port doc (RoleArrays §1, §9) records the swap reads `B[0]` for a
  slot past 10 ("PES reads B[0] for a slot past 10 and writes past B; GF refuses such a pair"). So PES
  does not bounds-check the pair against 11 — the check GF adds is stricter than PES.
- What differs: a swap request with either index ≥ 11 is dropped by GF and executed (modulo-wrapped or
  out-of-bounds, per the doc) by PES. The writer of the pair (`T+0x89D4/0x89D8`) is itself undecoded
  (RoleArrays open item 5; `RequestRoleSwap` has no caller — F5), so today no request can carry such a
  pair: the over-strict gate is unreachable.
- Effect: none today (no caller). When the pair writer lands, the first out-of-range pair would silently
  no-op where PES swaps.
- Fix: when the pair writer lands, replace the refuse with PES's rule (B[0] fallback + modulo, per the
  port doc §9) and cite `FUN_140A422C0`. No change now; the ponytail at `pesroles.hpp:58-61` already names
  the unported half.

### F2 — (Advisory) Head-write registry mirror lags `CalculateDynamicRoles` by up to 40 ticks (unmeasured)

- GF (`teamAIcontroller.cpp:3221-3231` `WriteRoleArrays(bool head)`): the head write (line 545 in
  `team.cpp`) mirrors each slot's **current dynamic formation role** into the sheet before
  `WriteSideRoles`. `CalculateDynamicRoles` runs every 40 ticks per team (`team.cpp` cadence), while the
  head write runs every team tick. So the mirror can lag a role change by up to ~40 ticks.
- PES: `FUN_1404F5F10` reads the sheet registry (roles per phase), not the live dynamic role — there is
  no lag because there is no live read. The lag is GF's registry-mirror design (ponytail at
  `teamAIcontroller.cpp:3216`, "GF has no sheet phases"), not a PES behavior.
- Effect: unmeasured; the RoleArrays port doc lists it as open item 8 ("Head-write timing ... Not
  measured"). Readers (`RoleClassOfSlot` → `BuildRunWorld` → off-ball gates, `DefensiveLineOf`,
  press class) can see a stale class for up to 40 ticks after a dynamic-role change.
- Fix (advisory only): measure once (log role-change tick vs reader-visible tick on a seed with dynamic
  roles), or move the mirror to read the same roster snapshot `CalculateDynamicRoles` writes. Not now —
  the ponytail names it and no test pins it.

### F3 — `WriteRoleArrays(false)` gates the tail swap on `possession && TailSwapAllowed`, PES gates on the latch + `T+0x255` (bounded, hunk-verified)

- GF (`teamAIcontroller.cpp:3234-3236` on main):
  `if (roleSwap && possession && pes::roles::TailSwapAllowed(roles)) pes::roles::SwapB(...)`.
  `possession` = `team->IsPesPossession()` (the `T+0x255` latch).
- PES (`FUN_14051E31E`, per RoleArrays §1): the tail swap runs gated on `T+0x255 != 0` and
  `FUN_140A41BE0` (the latch check = `TailSwapAllowed`). GF adds `possession` — but `possession` here
  **is** the `T+0x255` latch read (`IsPesPossession`), so the gate is `latch && latch-ok` where PES has
  `latch != 0 && latch-ok`. Identical. **Withdrawn as a divergence on re-read** — the two conjuncts
  coincide.
- Residual (kept as the finding): `roleSwap` itself (`RequestRoleSwap`) still has no caller and no
  decoded writer input (`T+0x89D4/0x89D8` OPEN, RoleArrays §9; `pesteamtick.hpp` ponytail). The tail swap
  never fires in GF. Bounded and correctly marked — see F5.

### F4 — `RankScore` gives the keeper-chase lift to the wrong men (bounded, one-line fix)

- PES (`FUN_14051B8B0`, per TeamSliceC §4 L206-249): score = height+jump+heading; the designated man
  (`P+0x3EF8`) is exempt; class 1 is −100 **whatever the keeper-chase flag says** ("the flag only lifts
  the rest" — port doc §4 line 1). I.e. under keeper-chase the flag lifts every non-designated man to
  his aerial **except class 1**, which stays −100.
- GF (`pesdesignation.cpp:28-42` on main):
  ```
  if (in.designated != k) {
    score = keeperFirst ? in.aerial[k] : in.lowRank;   // keeper-chase lifts EVERYONE to aerial
    if (cls == 1) score = in.lowRank;                  // ...then class 1 is dropped back to −100
  ```
  Net: under keeper-chase, class 1 → −100 ✓, everyone else → aerial ✓. **Matches.**
- Re-read verdict: the code matches the doc line-for-line (the `if (cls == 1)` arm runs after the
  `keeperFirst` lift, restoring exactly the "flag only lifts the rest" rule). **Withdrawn — no finding.**
- Residual kept in F4's place: `keeperFirst` is `in.keeperChase`, which `PesBoxMen` never sets (always
  `false`; `OrdinalInput.keeperChase` default, `teamAIcontroller.cpp:3303-3325` sets only
  side/onPitch/classB/aerial/extraFrontman/eligibleCount/boxCount/list — seven fields,
  `humanSide/takers/designated/keeperChase/restartMan` stay default). PES's keeper-chase
  (`FUN_140509890`) is undecoded (TeamSliceC open item 3). So the keeper-chase arm is dead code today —
  correctly marked by the port doc, but the seven defaulted stand-ins have no `ponytail:` at the call
  site (`PesBoxMen`), only in the port doc. Fix: add the ponytail at `teamAIcontroller.cpp:3303`
  naming `FUN_140509890`/`FUN_1408C5060`/`MI+0x173C/40`/`T+0x87CD` at the seven defaults. Comment-only.

### F5 — `DesignateTwoCf` has no caller; the `T+0x87CD` gate has no decoded writer (carried OPEN)

- GF (`pesdesignation.hpp:118-133`, `pesdesignation.cpp:224-234`): `DesignateTwoCf` is pure with its test
  (`PairArmedOnlyWhenGate2`); nothing calls it (verified: no `DesignateTwoCf` reference in
  `teamAIcontroller.cpp` on main). The port doc lists it as open item 5 ("not wired ... no reader of
  `T+0xB828/B82C/B830` was found").
- Status: unchanged since the port landed. Correctly documented; no code fix. Carried as the port's own
  open item, not a new finding. (Counts against "fully wired" but not against correctness.)

### F6 — The coordinator gate folds "no kick" and "opponent's kick" into one `false`, PES fails only on own-side valid `T+0x78` (narrower input, same verdict today)

- PES (`FUN_140632780`, decompile re-read this pass): FAIL iff `B+4 > 1.5` **or `T+0x78 < 0x16`**
  (`*(uint *)(param_2 + 0x78) < 0x16` at the decompile head, CONFIRMED). The test is on **this side's**
  `T+0x78` only (param_2 is the side's team record). An opponent's valid kick does not fail this side's
  gate.
- GF (`pesteamtick`/`pespress.hpp` `PressGateInput.teamKickValid`, wired at `teamAIcontroller.cpp:2519`):
  `kickValid = touchValid && touch.team == side` — true only for the side's own newest touch; false for
  no-touch **and** for the opponent's touch. `PressGatePasses` (`pespress.cpp:496-501`): fails when
  `teamKickValid`. So GF fails exactly when the side made the newest kick; PES fails when the side's
  `T+0x78` is valid.
- What differs: GF's `touch` is the newest of **either** side's ring-0 touches (loop `n < 8`,
  `:2513-2517` takes the first valid of either team), while PES reads only this side's `T+0x78` word.
  Case: side A kicked, then side B touched (newest = B). PES side-A gate: `T_A+0x78` still valid → FAIL.
  GF side-A gate: newest touch is B's → `kickValid=false` → gate does not fail on this arm (falls to
  cover/facing arms). The two disagree whenever the newest touch and this side's last kick differ.
- The `T+0x78` writer is OPEN (docs list it as the "last-kick id" with a live writer, VA open;
  `pes-re-press-coord.md` §1). GF's newest-touch stand-in is the documented substitute (ponytail at
  `:2508`: "the T+0x78 stand-in for this side's last kick"). Direction: GF's substitute is *newest-touch*,
  PES's word is *this-side-last-kick* — adjacent but not identical quantities.
- Effect: bounded. The gate is one of six PASS arms (press timer, flag 13, attack level ≥ 3, class 5,
  cover, facing ≤ 60°); the `T+0x78` arm matters only when all other arms fail **and** the newest touch
  is the opponent's while this side's own kick word is still valid. Rare, and the maintain step 6
  (`mi.teamKickMan = kickMan`, same newest-touch substitute) shares the input, so the two stay
  consistent with each other.
- Fix: when the `T+0x78` writer lands, feed each side's own last-kick word to **both** `g.teamKickValid`
  (`:2645`) and `mi.teamKickMan` (`:2607`) instead of the newest-touch pair; keep the newest-touch loop
  for the `touchValid`/`holderFromTouch` inputs that genuinely read the newest event. Two-line change +
  ponytail update at `:2508`.

### F7 — Maintain step 8 (the refusal `FUN_140633350`) is not ported (carried OPEN, bounded)

- PES maintain (`FUN_140632E70`, decompile re-read this pass): step 8 calls `FUN_140633350(P, T, m,
  B88C[k])` (refusal, PressEvalRE H42) — release on nonzero.
- GF (`pespress.cpp:520-545` `PressPresserKept`): step 7/8 line is a comment —
  "Step 7 (reason 3) is unreachable ... Step 8 (the refusal) is the eval's, not ported." Falls through
  to 9a. The eval-side refusal is likewise absent (`PressEvalPick` returns `accepted` without a refusal
  call — verified `:406-...` body has no refusal gate).
- Effect: bounded. The refusal needs `flag14(s,pm)`, the `P0 = FUN_14063FDC0` press point (OPEN tail H43),
  and the partner scan — three OPEN inputs. Porting the call now would invent its inputs. The port doc
  (FarChasePress2 handover §C item 2: "C2 second-key selection is unguarded" is the analogous
  already-accepted gap) and PressEvalRE §8 (O1/O2/O3) both leave it OPEN.
- Fix: none now. Port `FUN_140633350` with the press point (H23) when its tail (H43/O1) lands; until then
  the comment is the correct marking. No change.

### F8 — Coordinator class is always 5-or-7: `FUN_140632850` (class 4) and `FUN_140632320` (class 1) hardwired false (marked stand-in, bounded)

- GF (`teamAIcontroller.cpp:2582-2584`): only `clsIn.flagFiveMan` is fed; `flagDNone/testFun632850/
  teamB716/testFun632320` stay default false. So `PressClass` (`pespress.cpp:378-384`) returns 5 when
  the class-5 man exists, else 7. Classes 4/6/1 are unreachable.
- PES (`FUN_140632AE0`, coord doc §6): order 5 → 7 (flag 13) → 4 (`FUN_140632850`) → 6 (`T+0xB716`) →
  1 (`FUN_140632320`) → 7. GF collapses the middle to 7.
- What differs: a side that PES would classify 4, 6, or 1 dispatches the class-7 path instead (eval with
  mode 7 needs `aggressive`, else return — `:2664-2665`). The class-5 path (the common case: nearest man
  within 3 m of the ball) is exact, including `clauseOk = !lifted || !kickValid` (`:2575`, matches
  coord §8.8 "only if grounded or `T+0x78 > 21`").
- Effect: bounded and correctly marked (`pespress.hpp:301-303` ponytails "not decoded: caller").
  Class 4 needs `FUN_14062180` (OPEN, coord §9) + the cone scan; class 1 needs the no-carrier event gate
  (coord §8.4). Both need OPEN inputs — hardwiring false is the honest stand-in.
- Fix: none now. Port each predicate with its inputs when decoded. No change.

### F9 — The FK/corner/penalty run-to-ball still uses `FloorVelocity(approach)` above the walk band (Batch B residual, bounded, fix named)

- GF (`elizacontroller.cpp:515-522` on main): a free-kick/corner/penalty taker's run to the live ball
  uses `FloorVelocity(pes::kPieceTakerApproachGain * gap)`. The walk-down port (`dd9d6b4`, audit 5 F3)
  applied `walkDown_ms` (11 km/h from `DAT_14259BDFc`, image-verified `00 00 30 41` = 11.0 this pass) to
  the **set-piece walk** arm (`:187-188`) but left this **run-to-ball** arm on the gap rule, behind the
  ponytail "the emitter's `cmd+0x5c8` write is undecoded".
- PES: the run-to-ball for a taker is also a MOVE command through `FUN_1406B7560`, so the walk-down
  branch (`0x1406b77d6 call 0x140a76070`, verified bytes `e8 95 e8 3b 00` + staged result this pass)
  caps it at 11 km/h under the same gate (walk-speed doc §3.3). The gate inputs (`match+0x116c`,
  route flag, `FUN_140479470`, frame budget) are undecoded — the port doc and the code ponytail both
  name them.
- Effect: bounded (audit 5 F3's analysis stands): a taker far from the ball runs at sprint (up to
  8 m/s) instead of 3.0556 m/s, arriving up to ~2.5× sooner; within ~2.5 m both agree. The arm fires
  only for FK/corner/penalty takers chasing a live ball (rare: the ball is usually at the spot).
- Fix: apply `walkDown_ms` to `:520` as well (one-line: same ternary as `:187-188`), keeping the
  ponytail for the gate. Leave the carrier arm (`:188`) on the gap rule — the carry is root-warped,
  not a MOVE (walk-speed §2, DodgeGate §6).

### F10 — `GoalKickStandPoint` negates the clip offset's height where PES keeps it (bounded ~1 cm, one-line fix)

- GF (`pesgoalkick.cpp`, `GoalKickStandPoint` on main): `negated = (−x, +y, −z)` — "SetKick negates x
  and z (the sign mask `0x1425656D0`) and keeps the height". The comment cites the sign mask
  (`DAT_1425656D0` sign-bit flip, used by the bearing helpers) for a **position** negation.
- PES (`FUN_1406B3DB0` placer chain, per GoalKickPort §2 + goalkick-taker doc): the SetKick placer
  negates the lateral/forward offsets (x, z) and keeps the height — the height comment is GF's own
  (the hit record "has no root height, so the height change is zero", `GoalKickContactRoot` header).
  The sign mask `0x1425656D0` flips a sign bit; applying it to y would invert height, which no placer
  does. GF's code **keeps** y (`+clipOffsetFox_m.y` passes through `negated.y` unchanged) — the code is
  right, the cited mask is the wrong justification for a position (it is the bearing-fold mask).
- Re-read verdict: code correct (`y` kept), comment cites an unrelated mask. **Comment-only fix**: cite
  the placer (`FUN_1406B3DB0`) instead of `0x1425656D0` at `GoalKickStandPoint`. No behavior change.

### F11 — `UpdatePesDesignation` never refreshes the corner ordinals when eligibility changes mid-stoppage (bounded)

- GF (`teamAIcontroller.cpp:3303-3329` + `team.cpp:548`): `UpdatePesDesignation` runs every team tick on
  the PES path and rebuilds the **publisher record** (`Publish` → `pesDesignation`). `PesBoxMen`
  (corner box) reads `FillOrdinals` fresh at `PrepareSetPiece` time — so the ordinals are current at
  placement. But between placement and the take, `onPitch/eligibleCount` can change (injury, sub) while
  `fill.ordinal[]` (the SP[4+k] row the take reads) is not re-run.
- PES (`FUN_14051B8B0`): the ordinal fill runs at placement; the take reads the stored ordinals. Same
  shape — PES does not re-fill either (the record at `P+0x7CDC` persists). **No divergence found on
  re-read**: GF's per-placement `FillOrdinals` matches PES's per-placement fill.
- Verdict: **no finding** (withdrawn). The wiring test gap (TeamSliceC open item 1: "no red test for
  `UpdatePesDesignation` or `PesBoxMen`") stands as the port's own open item, not a new finding.

### F12 — (Advisory) `WriteRoleArrays` head-write walks `GetAllPlayers()[slot]` assuming slot order (fragile, assert named)

- GF (`teamAIcontroller.cpp:3223-3225`): `team->GetAllPlayers()[slot]->GetDynamicFormationEntry()` for
  `slot` in 0..10. Assumes the first 11 actives are in slot order.
- Today `GetAllPlayers()` returns roster order and all 11 slots are active at the head write (setup +
  every tick), so `[slot]` is the slot's player. If a slot goes inactive mid-match (injury/sub), the
  indexing shifts and the wrong player's role mirrors into that slot's sheet row.
- Effect: unmeasured; subs/injuries mid-match are the trigger. PES reads the sheet registry, not a live
  roster walk, so any shift is GF-only.
- Fix (cheap, defensive): index by slot (`FormationSlotOf` / slot-keyed roster) instead of vector
  position, or `assert(players.size() >= 11)` at the head write so the assumption fails loudly. One line;
  recommend the assert with the `FUN_1404F5F10` cite.

### T1 — (TurfEV advisory, carried) The `32·a²` alpha term is still omitted from the REDUCE gather (directionally safe)

- PES (`Tonemap_Lum.ps.asm` L5-12, TurfColour §2.3 CONFIRMED asm): `lum = max(32·a², max(Y, 0.001))` on
  the point-sampled sub-rect.
- GF (`godot/post/pes_post.glsl` REDUCE, on main): `value = max(dot(rgb,LUMA)*scale, LUMA_FLOOR)` —
  no alpha term (ponytail in the push-constant comment: "omits the `32·a²` alpha term (Godot's alpha is
  not PES's bright-pixel code)" — verified present on main this pass).
- Direction (TurfColour §2.3): `max(…, 32a²)` can only **raise** PES's metered mean, lowering PES's EV.
  PES's EV is already **higher** than GF's (the ~13× gap, §2.5), so the term works against the observed
  gap — omitting it is safe for the EV direction and bounded by the alpha writer being undecoded
  (TurfColour open item 4). No change.

### T2 — (TurfEV advisory, carried) Full-frame static rect still ships; the live comp-block rect writer is untraced

- GF (`pes_post.glsl` params `[6..9] = (0,0,1,1)` + `pes_lighting.cpp:80-88` full-frame static rect,
  ponytail "full-frame static inputs until the live comp-block rect is read" — verified present on
  main). PostPort A3 (quarter-rect gradient test) still open; GradePort/TurfEV none of the live
  `param_3+0x8c` writer decoded (ComposePath §4, TurfColour open item 1).
- After GradePort/SunPort/PostPort all landed, nothing changed the rect inputs: the finding is
  unchanged from PortAudit4 A3. No change; carried.

## Coverage list (checked and found correct)

**RoleArrays (MergeRA).** Class-map jump table re-verified from the image this pass: 13 RVAs at
`0x1408C52A4`, case bytes `41 ba imm32 / 41 8b c2 c3` with imms 0,1,2,2,3,4,5,5,6,8,8,7,9 — GF's
`LoadRoleClassMap` opcode check matches exactly (role 0 bare tail `41 8b c2 c3` → 0 ✓).
`RegisteredRoleOf` enumeration (GK0..CF12) matches the doc ✓. `WriteSideRoles` A=B bulk ✓ (both
assigned). `TailSwapAllowed` latch `< 11` ✓ (matches `FUN_140A41BE0` per doc). Every reader re-basing
from the port doc §3 re-checked at its GF site: `ChanceMaskReject` reach `cls < 4`, attack skip
`<= 4`, `AttackSlotMask` admit `>= 7`, `RankAngleOk` reach/angle, keeper `== 0`, `CounterInput`
`> 4 / > 2`, `StyleRunDepth`, `FramesToReach` keeper stand-in, `SubRoleClass` NOT re-based (correctly
left, port open item 1) ✓.

**TeamSliceA.** `StrategyWantLevel` ladder (0/1/3/4 → self, else 2) ✓. `ManualOverrideLevel` mesh
(2-passthrough, ==level passthrough, `<2`/`>2` arms) ✓. Freeze (`want != old && !inPlay → old`) ✓.
Clamp `min(want,4)`, no low clamp ✓. Stamp on change ✓. `WriteAttackFlags` (`>3`, `<1`) ✓.
`WriteRestartWord` (`!inPlay || flipped → 0xFF`) ✓. `StoppedSelect` gate (latch-clear, `!= 1`,
`== 5`) + store 2 ✓. `ClearReactionDelays` (`!= 1` clears both banks, returns bool) ✓.
`SupportWeights` (sign 1.0/−1.0, flip resets counter+stamp, bank = sign·weight in play / 1.0 out,
counter++ on no-flip, stamp −1 out of play, score-off fill 1.0 after the bank read) ✓.
`RestartKindOf` 1/2/3/4/5/6/0 ✓. `ArmInPlayCounters` (8, 4, done=false) ✓. Wiring order in
`UpdateStrategy` (8 → 9 → 11 → drift → flags → restart → counters → trap clear) matches the doc's
VAs ✓. `DefensiveLineHeight` B8A3-gated step-up wired ✓. `UpdateLaneLine` zero-input arms
correct-by-vacuity (audit 2 L2, unchanged).

**TeamSliceC.** Order table re-read from the image this pass: row 2 = (1,0), row 3 = (1,2,0),
row 10 = (5,4,6,3,7,2,8,1,9,0) ✓; `lowRank` −100.0 @`0x14259D674` ✓; `highClassBonus` 1000.0
@`0x14256AE34` ✓. `FoldClass` buckets ✓. `BucketByClass` + compact + stable insertion + DAT-rank
emission ✓. `FillOrdinals`: keeper-last-class-0 scan ✓, eligible-empty early return ✓, limit =
min(box, eligible) ✓, falling `next` from 10 ✓, stage-one bit-15/takers ✓, keeper-chase next ✓,
stage-two stable-descending score with keeper/placed/restartMan skips + store-then-limit-break ✓,
remainder tail 10..1 skipping element 0 + count gate ✓, ordinal-0 keeper rule ✓. `RankScore`
(designated exempt; keeperFirst lifts non-class-1; class 1 → −100; 2/10+ → 0; 3..9 aerial; 6..9
+1000) ✓ against the doc. Wiring: `UpdatePesDesignation` every team tick on the PES path ✓
(`team.cpp:548`); `PesBoxMen` PES-path branch + switch-off GF pick preserved (clean-cutover conflict
correctly resolved per the port doc) ✓. `DesignateTwoCf` pure + gate-2 + SS/CF filter ✓ (unwired, F5).

**Follow-ups.** `PrepareSetPiece` no-restart release (`ceb7991`) present ✓. DefensiveLine −14 m
level-up on team mode 3 (`416ce6a`) present ✓. Parity observer comment (`13c8456`) present ✓.

**FarChasePress2.** Coordinator-before-writer order (`PesPressTick`: `PesSecondPresserTick` then
`PesLooseWriterTick`) matches `FUN_140535C60` ✓. Holder = `GetPesHolder` (MA+0x2601C, persists on
loose) with key-actor fallback ✓ (press-coord §5). Class order 5 → 7 (others hardwired, F8) ✓.
Gate arms (timer/flag13/level≥3/class5/cover/facing≤60, ball-height 1.5, `T+0x78` arm F6) all present
with image-verified constants (1.5/2.5/14/11/140/60/45/81/8/15/10/4/9/25 all OK this pass) ✓.
Maintain steps 1-6, 9a-9f in order (step 8 absent, F7) ✓. Eval: table order + first-accept (no
keep-max, per PressEvalRE correction) ✓; θ ladder 22/15/13/10 +3 incumbent ✓; Block A ✓; accept-1/2,
G_sub, θ2 ✓; mode-7 (50/40) and mode-4 (100 + keepX margin) arms ✓. Loose-writer renewal+reset
lifecycle ✓ (press-life §10). Per-man press read (`PressOrDelay` over `pressState.role`) ✓.
Carrier-from-touch key push (`SetPesKeyActor` on `CarrierFromTouch == kToucher`, `:1338-1340`) +
holder latch (`:1349-1351`) ✓ with the `G+0x2A80` ponytail. Trap frame −z (`96529f7`, from the hit
records) — consumer code verified present via the port doc's wiring (elizacontroller press approach
below) ✓. ActionPress: target = ball-now-or-own-base (`PressTargetIsBall`, `:659-663` + `:905-909`),
pace = one-band-above curve (`PressCommandSpeed_ms`, `:914` + `:676`), command-led with
`useDesiredTarget` (MOVESTEP steers) ✓; tick = `fps·0.1+0.5` = 5 (54.0/0.1/0.5 image-verified) ✓.
Designated-man stand-off on the PES path (no GF chase step, `:623-624`) ✓.

**Parity + press-layer.** Observer-only, determinism-neutral (its own report's F1–F8 are doc/log
findings, no code divergences to audit) ✓. Press-layer port doc committed no code by design; every
helper it deferred (`FUN_140535260/4567e0/a42940/a6ce10/a53f90/a4d660/a603c0/a62ea0/a62610/
a57720/a6e150...`, `FUN_140639830/9a20/9600`, picker tails, writer tables) is now superseded by
PressCoordRE/PressEvalRE/PressLife decodes, whose GF consumers are audited above ✓.

**Batch B (highest traffic, not in audit 5).**
DodgeGate body-frame cache (`TurnCacheToBody` at both install sites, `:530`/`:566` +
`GetRotated2D(bodyAngle)`) ✓; `ResetPosition` idle anchor ✓; held-taker MOVESTEP exemption ✓;
corner SetKick heading (315/45 + restart yaw, `CornerSetKickHeadingDeg`) ✓; goal-kick SetKick yaw 0
✓; pickup contact-root gate (5 m, `DAT_14259222C`) ✓; carry lift (0.65 m, 14 frames) ✓;
`HoldsRoot` lift-scoped (audit 3 R4's "delete" superseded by `475e237` with measured cause: 2/2
lifted takers threw out without it; walker-at-0-m/s rejection documented; wire test still pins
behavior) ✓. Walk-down 11 km/h on the walk arm (`:187-188`, `DAT_14259BDFc` image-verified) ✓
(carrier + FK-run arms excepted, F9). TSheetPort chooser: kind ladder (32/60/28/65/−1, all five VAs
image-verified this pass), loop gates, repick gate, rating fallback — all match the doc ✓.
GoalKickPort: taker = keeper ✓; lofted-always ladder (waits/ windows/ forced, `pesgoalkick.cpp:60-100`)
✓; picker best-rating (stand-in, ponytail'd) ✓; bisection solver on PES's own integrator ✓;
1.5 m receive height (OBS, ponytail'd) ✓; `36·(power+0.3)` touch shared with `humanoid.cpp:811` ✓.
ClipWords fill (`FillMoveWords` req words per the table, walk-down call site bytes verified
`e8 95 e8 3b 00`) ✓. SeamWire (`NearestOpponentOf`, `RunSlotOf`, parity-frame staleness documented)
✓. SwitchSW pes_loop switch ✓ (presence-verified). MotionRecords single-read (`Motion()`,
`LoadMotionCaps`) ✓ (presence-verified). StylePort gate (compat rows, `Gate`, `StyleLb` pes-path)
✓ (presence-verified). LanePort core (`ScanLane`, `LoadLaneConstants`, one-touch listed-not-ported)
✓ (presence-verified). StepFamily TURN_STOP/RUN_TO_STEP fixes ✓ (presence-verified). S1Port
NearestList consumers ✓ (presence-verified). Offside constants ✓ (presence-verified).
SignatureAnims derivation tables ✓ (presence-verified). PenaltyScatter/Integrate (radius + jitter,
draw order lateral-first, `max(0,height)`) ✓ (presence-verified).

**TurfEV after GradePort/SunPort/PostPort.** Exposure law (`FUN_1416c7930` line-for-line, key
0.18/comp/min/max/tonemapSpeed/stepGain plumbing `:60-88`) ✓ unchanged. Grading pair literals (all
14: 0.0031308/12.92/1.055/0.055/2.4/1e-5/0.25/4.0/0.947867334/0.00982/4.0/12.92 + stub+display
identity) ✓ unchanged. Sun state (lux = sunLux·fade, no 1/π; π·S colour + sRGB; fade knees;
`SkyRadianceScales` lux·π·S) ✓ unchanged. White-balance bypass (CubeLut stub never samples;
`Tonemap` film path not bound in matches per ComposePathRE) ✓ unchanged. Turf albedo chain
(Withers/mask/AO lanes, asm 44–152 cited in-shader) ✓ unchanged; no `FIN_NET` on main (TurfFin
correctly never landed) ✓. Moon advisory (raw lux, day-hidden) ✓ unchanged.

## Open questions (no port impact today)

1. `T+0x255` latch vs EC600-arg4 `F` in `WriteRoleArrays` head/tail flags (audit 1 F6's parity
   sub-path; audit 5 F7): still unported, still ponytail'd (`teamAIcontroller.cpp:3216`). Alternation
   per frame once `MI+0x119C > 1` not modelled.
2. `T+0x210` manual word, `+0x8D/+0x1FC` gate chain, `T+0x6E6` gate: still OPEN, still ponytail'd.
3. The `+0x5EC` head-point writer (`FUN_14093CB6C` family): F2 of audit 1 stands.
4. `T+0x78` last-kick writer: F6's fix waits on it.
5. `FUN_140509890` keeper-chase, `FUN_1408C5060` sheet takers, `MI+0x173C/40` restart men,
   `T+0x87CD` writer, `T+0xB828/B82C/B830` readers: F4/F5 waits on them.
6. `FUN_140633350` refusal + `FUN_140A51D50` P10 tail: F7 waits on them.
7. `FUN_140632850` class-4 + `FUN_140632320` class-1 predicates: F8 waits on them.
8. TurfEV: live composition path (grading LUT bound in matches?), sun-radiance normalization (~13×),
   per-material residual (grass ~2× red at matched EV), `32·a²` alpha writer, 15:00 span assumption —
   all carried from TurfColour §5 / PortAudit4.

## Remaining-unaudited `docs/ports/` reports on `origin/main` after this pass

Covered by audits 1–7: RoleArrays, TeamSliceA, TeamSliceC (Batch C); FarChasePress(Main+2),
Parity, press-layer (Batch C); Forfeit+minimum-players, TeamT1/T2 slices, walk speed+RootStart,
holder latch+keeper held, mentality first rules, ZoneJudge+PlayerDB (audit 5); StrategyFix,
CondTactics (audit 6); SunPort, GradePort, PostPort, TurfFinPort, exposure+b2 (audit 4);
DodgeGate(+clip frame/lift/pickup/approach-revert), GoalKickPort, MoveStepWire, SwitchSW,
MotionRecords, TSheetPort, ClipWords(r1–r3), RestartRef(1–5, via InPlayRelease2/ZoneJudge),
ThrowInTaker(PathB placer), SeamWire, StylePort(+StylePort2 effects), StepFamily,
LanePort(+CrossLane wiring), NearestList, PenaltyScatter(+Integrate),
SignatureAnims, FirstTouch(port+repro state), A5Repro+A5Tail2 (repro/blocker reports),
ShootVerdict (diagnosis), SpinRunaway(+2), BugFix, ComStyleGaps, NullHazards, A2Wiring,
B6Stadium, DeadBall, IdleEpisodes(+2), A4UnitsFix, IdleEpisodes-2, offside-port,
offsideawareness, A1Spin, A3States, A4LaneInputs, A4Lanes(2,3), A4Level, A4Units, A4Walk,
A5Port(+blocker), A5Tail, A6Breaks, A6Whistle, AccelRamp, StockConst, ThrowInTaker(doc),
SeamWire, CrossLane, S1Port, DefensiveShapePort, keeperport2, PenaltyIntegrate,
FpkBounds, Hygiene, A5Merge, balltick-state, carriergainfix-state, carrierimageport-state.

Not yet covered by any audit (7 reports + 3 doc-only sets):

1. `docs/ports/pes-bisect-accuracy.md` — the lofted-bisection accuracy relation (solver test
   support; small, numeric).
2. `docs/ports/pes-a1t2-Liftoff.md`, `pes-a1t4-Liftoff.md`, `pes-a1t4b-Liftoff.md`,
   `pes-a1t4c-Liftoff.md` — the A1 liftoff series (ball-launch basement; 4 short reports).
3. `docs/ports/pes-a3-salvage-table.md` — the A3 salvage table.
4. `docs/ports/pes-doc-propagation.md`, `pes-doc-propagation-r3.md`, `pes-doc-propagation-r4.md` —
   doc-propagation bookkeeping (process, not a port; confirm-and-file).
5. `docs/ports/README.md` — index (no audit needed; verify it lists the above).

Suggested next (highest value first): (1) land F4-ponytail + F10-comment + F12-assert (three
comment/one-line items, no behavior change); (2) apply F9 (`walkDown_ms` to the FK-run arm) with the
named gate ponytail — the last open Batch B speed item; (3) audit the A1 liftoff series + bisect-accuracy
(closes the ball-launch basement); (4) decode the `T+0x78` writer (unblocks F6 and the maintain-6 input
in one move).
