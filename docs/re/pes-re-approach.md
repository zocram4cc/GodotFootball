# PES approach decode: why GF's near men stand where no trap clip fits (ApproachRE, 19-11-26)

Status: decode + GF probe + one mutation. No engine code landed; no commit. Branch under study: r18 (`<workdrive>/gf-AnimeLayer`, uncommitted, base `8a65de72`). Snapshot used: r18 at 06:44 on 19-11-26 (Main keeps editing r18; line numbers in section 2 are given for the snapshot and for the r18 tree as checked at the end).
Tags: **CONFIRMED** = read in the image (VA-cited, verify script) or measured in the GF probe; **INF** = inference (reason given); **OPEN** = not settled (blocker named).
Scratch: `<workdrive>/gf-ApproachRE-scratch/` (`verify_approach.py`, `analyze_probe.py`, `probe-run.log`, `probe2-run.log`, `mut-run.log`, `r18-worktree.patch`). Probe worktree: `<workdrive>/gf-ApproachRE` (detached, `8a65de72` + r18 files + probe edits, NOT for landing; Main removes it).

## 0. Constants (script output, `verify_approach.py`, read from `pes_full.bin`, file offset = VA - 0x140000000)

```
1.0 flat      0x14256ae1c  f32 1         bits 3f800000   (flat control, CONFIRMED)
100.0 C1      0x14253ee30  f32 100       bits 42c80000
0.5           0x142565600  f32 0.5       bits 3f000000
0.1 press k   0x142588200  f32 0.100000001 bits 3dcccccd
10.0          0x142588210  f32 10        bits 41200000
5.0           0x14259222c  f32 5         bits 40a00000
0.05 class0   0x1425a8a28  f32 0.0500000007 bits 3d4ccccd   (km/h)
8.0 class1    0x14259bdf4  f32 8         bits 41000000     (km/h; also the 8 m top-candidate radius)
13.5 class2   0x1425a1ffc  f32 13.5      bits 41580000     (km/h)
22.0 class3   0x1425c48c0  f32 22        bits 41b00000     (km/h)
solver thr    0x1425e4a28  f64 1e-06                       (double, used by FUN_1406ECBB0)
```

## 1. PES: how a non-holder reaches a resting loose ball, and what the trap needs

**1.1 Who approaches (CONFIRMED structure, doc `docs/re/pes-re-press-life.md` §3–§5, §10; `pes-re-first-touch.md` §7.1).** A loose ball gets one PRESS (0x34) man from the press writer `FUN_140536FC0` (+ a DELAY man). Candidates are inside d² < 81 m² (9 m, `0x1425C6780`); the top candidate must be < 8 m (`0x14259BDF4` = 8.0, verified above). The writer runs only with no carrier (MA+0x2601C ≥ 0x16, doc first-touch §7.1). MATCH_UP (0x3A) is the carrier's mark, not the loose-ball chaser (first-touch §7.5).

**1.2 Target (CONFIRMED structure, press-life §7; pdata chunk NOT re-verified here, OPEN-V).** `FUN_140639100`, no-carrier arm: k = int(0.1·54 + 0.5) = 5 ticks; target = ball-now if |S_k − B|² ≤ |S_k − A|², else the presser's own base (AI+0x1414). For a resting ball S_k = B, so the target is the ball. Carrier-valid arm: the carrier's ball point with a lead.

**1.3 The man (VERIFIED begin and operands, `FUN_140962440` ActionPress).** Runs every frame for a PRESS man. `0x140962695 COMISS XMM0,[0x142588210]` (10.0, the score/arrival knee); `0x14096293C ADDSS XMM6,[0x142565600]` (+0.5, the peer-tie margin of step 7); `0x140962AA0 MOVSS XMM2,[0x14259222C]` (5.0, the touch-frame step). Target = §1.2. Speed/ETA = planner `FUN_1406788B0`: `max(1, int(fps·dist))` ticks (press-life §9, CONFIRMED structure). **No distance arrival test inside ActionPress**: it stops only when its state leaves 0x34 (press-life §8, end; §10).

**1.4 Speed (CONFIRMED structure, press-life §8–§9; FUN_140A75BE0 has no .pdata entry, its cuts are verified as constants only).** PES's command speed is not 2.6·distance. The planner takes the man's speed class `FUN_140A75BE0(pl)` (cuts 0.05 / 8 / 13.5 / 22 km/h, §0) and returns an ETA ≥ 1 tick. The planner never returns a zero command toward a target the man has not reached (INF high from the `max(1, …)` clamp; the clamp's operands are in `FUN_1406788B0`, not re-read here).

**1.5 Contact (VERIFIED, `verify_approach.py`).** Reaction entry 3 `FUN_1409F8220` → `FUN_1409F8640`:
- TRAP admission calls `0x1409F878D` and `0x1409F880D` → `FUN_1406B4580` (try / retry; begin `0x1409F8640` is a .pdata begin).
- Refusal gate `0x1409FA4F7` → `FUN_1409FA520`; inside it `0x1409FA550` → `FUN_1406B4C10` (TRAP-admissible), and `0x1409FA57B CMP dword ptr [RBX+0x2600C],ESI` (designated owner MA+0x2600C == me refuses).
- Reach: `0x1409FA480 COMISS XMM0,[0x14253EE30]` (C1 ≤ 100 m², i.e. 10 m).
- No height, facing or speed test on the TRAP branch (walker-touch R2).

**1.6 Clip choice and the solver (CONFIRMED structure from r18 headless decompiles `solver.c`; request-word origins OPEN).** The kind-0 chain `FUN_14079A470` (trap-kind0 doc) runs race refusal `FUN_140799440`, then the candidate query, then commit `FUN_14079AFD0`; fallback `FUN_140857220` → `FUN_140858B90`. The clip solver `FUN_1406ECBB0` (decompile lines 283–470) picks the per-axis stretch limit from request words `[0xd]`, `[0xe]`, `[0xf]` by the sign of the root velocity and by whether |v| ≥ DAT_1425E4A28 (1e-6, §0). A man standing still (|v| below 1e-6) takes the sign branch `[0xd]` or `[0xe]`; a moving man takes `[0xf]` when v+Δ keeps its sign. Consequence (INF): the stretch the clip may use depends on whether he arrives moving. The source of the request words is the candidate query `FUN_14084ECA0` (class 0x15), OPEN.

## 2. GF: the rules that leave the man standing (MEASURED with a probe; r18 file:line)

Probe: `[approach]` rows (every man within 1.6 m of a ball under 0.5 m/s, each tick) and `[blend]` rows (inside `_MovementCommand`), two 2-minute seed-7 runs of `PesPassTarget.IsSetInFlightAndNeverStaleWhileTheBallRests` on the snapshot. Worst stale: 8.57 s (bound 2.94 s). Row counts: 10,031 approach rows, 2,987 blend rows.

**G1. The approach speed is quantised to zero inside 0.69 m. (CONFIRMED, measured; the quantiser is the rule to change.)**
- `src/sim/onthepitch/player/controller/elizacontroller.cpp:958` sets the loose presser's speed to `pes::PressCommandSpeed_ms` (= min(2.6·dist, one class band above current), `src/sim/onthepitch/pespress.cpp:603-613`, slope `distanceToVelocityMultiplier` = 2.6, `src/sim/gamedefines.hpp:25`), then **`:962` `inputVelocityFloat = RangeVelocity(inputVelocityFloat)`**, "emulate controller quantization" (`src/sim/onthepitch/velocitystate.hpp:27-36`: 0 below 1.8; 3.5 for 1.8–4.2; 5.0 for 4.2–6.0; 8.0 above).
- Measured: presser man 26, t = 104.85–104.94 s, distance 0.64–0.68 m, ball 0 m/s, useDesiredTarget = 1, target = ball, `manualV = 0`, `autoBias = 0`, `outV = 0` (`probe2-run.log`, `[blend]`). 2.6 × 0.66 = 1.72 < 1.8, so the quantiser returns 0.
- Count (probe2): 91 of 1,253 ball-targeted approach rows at rest within 1.6 m have commanded speed 0, **all at dist < 0.75 m**; 66 blend rows with outV = 0 at dist ≥ 0.5 m.
- Mutation (scratch only: line 962 skipped for the loose presser, `mut-run.log`): zero-speed approach rows 91 → 0; worst stale 8.57 → 8.52 s (single seed, noisy). **So G1 is a real stop rule but it is not what keeps the ball loose for 8 s.**
- PES has no such stop: §1.4 (ETA ≥ 1 tick, no quantiser).

**G2. The approacher is commanded fast but moves slowly. (MEASURED; cause OPEN.)** Man 26 at 1.25 m: `cmdV 3.5`, `useTgt 1`, actual speed 0.14–1.54 m/s over t = 5–115 s. The movement solver (`src/pes/movestep_*.cpp`, `pesmovestep.cpp`) does not follow the command at these distances. OPEN: which MOVESTEP gate (class, precise flag, turn) holds him. Not decoded here.

**G3. The holder latch keeps one man refused and standing. (MEASURED; the PES fact behind it is INF.)**
- `Match::RecomputePesHolder` (`src/sim/onthepitch/match.hpp:278-300`): "the holder has no clear of its own"; it adopts the newest ring-1 key only, and the key is pushed only by a touch that leaves the ball at the toucher's feet (`humanoid.cpp:1395, 1503`; `CarrierFromTouch`).
- In the probe, PES holder `(team 0, id 9)` (restart taker) sits 0.4–1.0 m from the resting ball, `holder=1 desig=1`, `cmdV 0` with no target, for rows t = 6.3–332.9 s (499 rows); `(0,10)` at 0.8–1.6 m for t = 218–354 s.
- He is refused his own trap: `playercontroller.cpp:416` (`GetPesHolder() == me && !CarrierMayTouch`) and takes the carrier brain instead of the step: `elizacontroller.cpp:657-658` (`hasIt`, r18 current; 637–638 at the snapshot), so `GetOnTheBallCommands` queues nothing for a ball 1 m away.
- PES fact behind the persistence (GF's comment, `teamAIcontroller.cpp:2616`): "it persists on a loose ball, so the reaction keeps its man while the ball is in transit". OPEN: whether PES's holder stands 1 m from a resting ball at all (PES's K=8 key or the W1 recompute may clear him; not decoded).

**G4. Non-approachers stand. (MEASURED.)** Non-PRESS, non-designated men (`desig 0, holder 0`) get `Movement`, `cmdV 0`, no target on 826 rows (`analyze_probe.py`). The command comes from the role strategies (`elizacontroller.cpp:603-618`, `defenseStrategy/midfieldStrategy/offenseStrategy->RequestInput`). PES gives these men a MOVE base; whether that base is 0.9–1.2 m from a resting ball is OPEN.

**G5. The trap solver refuses those standing men (CONFIRMED from the earlier decode, `docs/re/pes-re-trap-kind0.md`; statuses 3–12 measured in r18).** A standing man has |v| below 1e-6, so he takes the sign-branch stretch limit (§1.6). That is the "no clip fits at 1 m" of the brief.

## 3. Verdict and the GF rule to change

Three GF stand-ins keep a loose ball untouched for 5–20 s:
1. **G4 (primary, INF):** the men beside the ball are never sent in. Only one PRESS man approaches, and the others run the role strategies that stand them at 0.9–1.2 m.
2. **G3 (CONFIRMED measurement; PES fact OPEN):** the PES holder stays latched on a man 0.4–1.6 m from the ball and is both refused and held in the carrier brain.
3. **G1 (CONFIRMED, measured):** the one approacher stops dead at 0.64–0.69 m because the press speed is passed through `RangeVelocity`.

GF rule to change (PES-faithful direction, for Main to decide): replace the loose-presser speed at `elizacontroller.cpp:958-962` with the PES planner semantics (ETA = max(1, fps·dist), speed from the man's class; no 2.6·dist law and no quantiser on the PES presser), and resolve G3 from PES's own clear of MA+0x2601C (OPEN, decode `FUN_140457070` W1 clears).

## 4. Red-test spec (PES facts; oracle fails on r18)

Test: `PesPassTarget.PressApproachNeverStallsShortOfARestingBall` in `tests/sim/pes_pass_target_test.cpp`, same 2-minute seed-7 PES-loop match as the stale test.
- Scenario: in play (`PesPlayState() == 1`), ball at rest (< 0.1 m/s), one loose-presser man `IsLoosePresser(player)` at 0.5–1.6 m from the ball.
- Assertion: on every tick that the queued movement command for that man targets the ball (`useDesiredTarget`, target = ball) and his distance to the ball is > 0.5 m, the command's `desiredVelocityFloat` > 0. Expected count of violations: 0.
- PES fact: `FUN_1406788B0` returns `max(1, int(fps·dist))` ticks (press-life §9), so PES never issues a zero approach toward a target that is not reached.
- r18 result (measured, probe2): 91 violating ticks, all at 0.64–0.68 m → **red**.
- Mutation that must turn it red: the r18 line `elizacontroller.cpp:962` (already red). Removing the quantiser for the presser (my mutant): **green on this assertion** (0 violations), which is the expected direction; the stale bound stays red (8.52 s), so this test pins G1 only.

Second spec (not written, OPEN until PES's W1 clear is decoded): a holder 0.5–1.6 m from a resting ball for > 5 s with no trap by any man must lose his holder status. No red test is written for it; the PES fact is missing.

## 5. Verification

- `verify_approach.py` (scratch): **CHECKS 16, FAILURES 0** on the cited VAs: flat control (f32 0x14256AE1C = 1.0); `0x1409F8640` begin; calls `0x1406B4580` at `0x1409F878D` and `0x1409F880D`; `0x1409FA57B` `cmp [rbx+0x2600c]`; refusal call `0x1409FA4F7` → `0x1409FA520`; `0x1409FA550` → `0x1406B4C10`; `0x1409FA480` comiss vs `0x14253EE30` (100.0); `0x140962440` begin; `0x140962695` vs `0x142588210`; `0x14096293C` vs `0x142565600`; `0x140962AA0` vs `0x14259222C`; the constants 100.0 / 10.0 / 0.5 / 5.0. Every VA is in a .pdata range and an instruction start of the linear decode from its range begin (capstone). Note: 0x1409FA57B is in its own .pdata chunk (0x1409FA573–0x1409FA591).
- Mutation: `verify_approach.py --mutate` moves the `TRAP admit (try)` site by one byte → **MUTATION CAUGHT** (only that check fails).
- Not verified by the script: `FUN_140639100` (its no-carrier constants are not in the .pdata range `0x140639100–0x140639254`; chunk OPEN-V), `FUN_140A75BE0` (no .pdata entry), `FUN_1406ECBB0` operands (decompile only; the solver body spans several .pdata chunks).
- GF probe and mutant: measured in the scratch worktree; the run is one seed. Not gated, not committed; `tools/dev/gate.sh` not run (Main gates).
- Machine: one `simslot.sh` slot per run; builds under `/tmp/gf-build.lock`.

## 6. OPEN items (each names the settling step)

1. Request words `[0xd..0x12]` of the clip solver: origin (`FUN_14084ECA0` class 0x15 query). Settles G5 numerically. Needs a decode of the query builder.
2. PES's presser stop/arrival: the MOVESTEP reading of his ETA command at 0.5–1.5 m, and whether PES's PRESS man ever stands short of contact. Needs the live hook in §7.
3. Holder clear: PES's W1 recompute (`FUN_140457070` at `0x14045725A`) and the K=8 key writers decide when MA+0x2601C changes on a loose ball. Settles G3.
4. Non-presser bases: PES's MOVE base for a man beside a resting ball (G4). Needs a capture of a restart, or the formation writer decode.
5. G2: why a commanded 3.5 m/s presser moves at 0.1–1.5 m/s at 1 m (MOVESTEP gate). Needs the pesmovestep trace for one man.
6. `FUN_140639100` no-carrier constants (0.1, 0.5) and `FUN_140A75BE0` operands: pdata chunk and function body to verify.

## 7. Live hooks wanted (none started; Main owns the rig)

One hook, if Main wants the PES side measured:
- `press_action_140962440`: entry of ActionPress `0x140962440`, prologue `488bc448895820555657415441554156` (image bytes), format as in `pes-port-PassArrival.md` §7 (`hook <name> <VA> 17 1 1 <prologue>`, Main to adapt the argument fields). Predicate: the man's action is 0x34 and the ball sample is at rest (< 0.1 m/s) with the man 0.5–1.6 m from it. Cap 24, FULL. Answers OPEN 2: the ETA/target PES's presser gets when he is that close, and whether PES's PRESS man stands short of contact.

No other hook is needed for this report.

## 8. Notes for the reader

- The stale number is single-seed and noisy (8.5–8.6 s in every run here; the mutant moved it 0.05 s). Do not read G1's mutant as a fix.
- The `hasIt` PES-holder change kept on r18 (`elizacontroller.cpp:657-658`) is what makes G3 bite: the holder never leaves the carrier brain. It is not wrong in principle, but it needs the holder clear (OPEN 3) to be complete.
