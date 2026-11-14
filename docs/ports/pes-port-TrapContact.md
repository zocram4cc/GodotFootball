# PES port: TrapContact (branch TrapContact, origin/main 2d0759c4 + origin/WhistleBall 6f6e7ce0): the body contact scan FUN_1406EDCE0 decoded, translated, proven (1 REAL + 8 SYNTHETIC calls), wired on the PES path; the touch gate is now PES's reach test; acceptance NOT closed (09-11-26)

Image `pes_full.bin`. CONFIRMED = decompile / asm / tool run this session; INF = inference; OPEN = blocker named. Pushed tip: see section 8 (`git ls-remote origin TrapContact`).

Image `pes_full.bin`. CONFIRMED = decompile / asm / tool run this session; INF = inference; OPEN = blocker named. Pushed tip: see section 8 (`git ls-remote origin TrapContact`).

**CORRECTION found last (read this first).** The synthetic `reach_hit` cases did NOT pass the reach test: the committed layer's `FUN_140799060` carries `OUT-OF-SET CALL 0x1406B0540` (the hit-offset reader) and `0x1406E6CE0` (the mode counter), which no replay entered, so the reach test's pass path and the joint-block branch are **unproven and untranslated**. My python prediction used hit bone 6; the real bone comes from `FUN_1406B0180(db, motion, flag, hit index)` (flag / index inputs from the interface slots 0x30 / 0x40) and the replays took the non-joint branch (`FUN_141EB5F00`) and failed the distance test. With the owner byte fixed to none, a pes-loop seed-7 run now ABORTS at the first contact frame: `PES_DISPATCH_UNTRANSLATED: 0x1406b0540` (smoke_new3.log). **The branch tip is therefore a CHECKPOINT that aborts on the first PES trap contact; do not gate or land it.** Resume: (a) add `0x1406B0540` and `0x1406E6CE0` as roots (`closure.py --members-file`), (b) fix the synthetic cases so the reach test passes (read the hit bone and index from the interface slots in the capture instead of assuming bone 6 / index 0; 1406B0540 has 8 SYNTHETIC cases on branch KickMotion, KickJoints2), (c) re-prove EDCE0 (reach pass + touch chain), regenerate, survey again (expect `0x1406E6CE0` and others), then sections 6-7.

| ask | state |
|---|---|
| 1. decode FUN_1406EDCE0, translate with its closure, prove it | **DONE**: section 1; `0x1406EDCE0` 5/5 bit-exact (1 REAL call `live2/pid3332804/seq040`, 4 SYNTHETIC), own-block 57/126 = 0.4524; an earlier run proved the other 3 synthetic cases (reach miss, touched flag, foot sensor) 4/4. Layer regenerated: 14 new functions (closure_trapscan.json, 51 bounded members), ledger current. Survey (seed 7, pes-loop): empty. |
| 2. feed it from GF, replace the `touchframe` touch on the PES path | **DONE in code, behaviour NOT validated** (sections 3, 5): joint block from GF's skeleton, scan after the trap tick, the humanoid's PES-trap touch is the scan's grant. First run (owner byte = PES holder) refused every reach test (28 of 28: the owner byte named another man); the fix (owner = none) is built but its smoke result was not read before the budget ended. |
| 3. red test, mutation check | Tests WRITTEN (`PesStall.ATrapTouchIsTheBodyScansGrantAndNothingElse`, `PesStall.ATrapReachesABallAtRest`, tests/sim/pes_stall_test.cpp); **not run, no mutation run** (section 6). |
| 4. smoke moves, drifted seeds, `GATE_TIER=branch` | **NOT DONE** (section 7). |
| acceptance: stalls gone or traced | **Traced, not gone**: the scan's reach test is stricter than GF's gate, and GF's body does not place the foot where PES's displaced pose does (section 4). |

## 1. What FUN_1406EDCE0 is (CONFIRMED)

- Only caller: `0x1409F1640` (per-frame update of the player object: `FUN_1406EDCE0(P = *(obj+0x7D8), out = *(obj+0x19E88))`, then `FUN_1404309C0` when the record's `+0x10D` bit 0 is set). It runs every frame for every man.
- Reads, in order:
  1. `FUN_141EB54F0` (`P+0xA08` anime chain, `*(A+0x138)+0x58` bit 4): gates the body-vs-body block.
  2. Body-vs-body: the root collision object `P+0x3AA0`'s contact list; a kind-7 record with A id != B id sets bit `rec+7` (< 16) of `P+0x2474`.
  3. Ball sensor: ball-hit capsule set `P+0x3AA8`; a bone b with `P+0x2704[b] == 2` and a ball contact on it (`FUN_14081D8F0(set, b) != 0` or a kind-1 root record mapped to b) is `bVar7`, a physical touch. Hands (bones 14, 18) take the `P+0x2460` bit 8 branch.
  4. The Trap action sets its bones to mode 12 (no collision): a real in-trap man reads modes `1..6, 12..14, 16..18 = 12`, rest `0x11`. **A trap never gets a physical ball contact; its touch is not the capsule solver's.**
  5. The reach test. With `P+0x2460` bit 9 (0x200, contact pending: set by the trap tick on the clip's contact frame), no sensor touch, and `ball+0x279C` >= 0x16 or == his id (`0x1406EDE54`, `0x1406EDE69`), the scan calls the current action's slot 23 (`[action+0xB8]`). The Trap action (vtable `0x1425E1ED0`) has `FUN_140799060`:
     - hit bone `b = ((motion hit[0].word0 >> 27) & 31) + 1`, motion record = `[[0x1436F23C0]] + motion * 0x390`, hit record at `+0x20`;
     - predicted ball centre = joint-block position `P+0x538+12b` + joint-block rotation `P+0x700+16b` applied to (hit words 0x16, 0x17, 0x15 in cm) * 0.01 * `P+0x3B18` (z negated when the motion is mirrored) + a per-motion constant displacement (`FUN_14085C1B0`, nine motions);
     - touch iff horizontal distance to the ball's sample 1 (`ball+0xB0`) < (reach(b) + 0.1086859)^2 (`0x142598958`), |dy| < 0.3 (`0x142590D98`), and some bone is not in mode 0x11 (`FUN_1406E6CE0 > 0`); reach(b) is the table of `FUN_140798240` (0.15 .. 0.3 m);
     - a pass sets `P+0x2460 |= 0x400`.
     The scan clears bit 9 either way.
  6. With `P+0x2460 & 0x480` (the global `DAT_14344E264` is 1 in the image and read only here) the scan runs `FUN_140717ED0 / 1406E3210 / 1406E47A0 / 1406E2EA0` and fills the record: `+0x58` kind 4, `+0x9C..+0xA4` a vector, `+0x10D` flags, then `P+0x2460 = (flags & ~0x400) | 4`. Otherwise it takes the restart-touch path (`FUN_140A63140`, `MI+0x116C == 3`).
- **The ball's new velocity is not the scan's**: the record goes to the ball's kick application `FUN_140870A40` (only caller `FUN_140425010`; it calls `FUN_140874860`), NOT translated here.
- Real-state check (RestingBall3 `ee_pid1010053_153`, a man in the Trap action, hit bone 6 = foot_r): predicted contact y = 0.1102 against the ball radius 0.1087, i.e. the predicted point is the ball's centre on the ground; the real ball was 0.45 m (horizontal) from it: refused. The joint rotation applied to the offset needs no extra frame correction: the model reproduces the ball radius height.

## 2. Translation and proof (CONFIRMED)

- `closure.py --spec-set trapaction --tag trapscan` over 18 roots was dry-run (770 static members); the static closure's specs were NOT kept (first attempt widened every trapaction record's universe to 351 functions and `gen_translated` aborted on `0x140721800` / `0x1415A05B0`, which hold unsupported instructions): only the root's spec stays, the bounded proof's 51 members come from `closure_trapscan.json`. Lesson for the next one: `closure.py --members-file` with the root only.
- `synth_trapscan_specs.py` / `synth_specs/trap_scan.json`: 8 cases over a real in-trap state (P = rdx of `ee_pid1010053_153`; mod-era capture, valid for a translation proof): `reach_hit` (ball placed on the predicted contact point, computed from the capture's own bytes), `reach_miss`, `reach_high`, `reach_other_owner`, `touched` (0x400), `forced` (0x80), `sensor_foot`, `sensor_hand`. Output record placed above the call's rsp, zeroed.
- Replays (`prove_bounded.py`, `snapdiff --bounded`): batch 1 (reach_hit, reach_miss, touched, sensor_foot) 4/4, own-block 0.4286; batch 2 (reach_hit, reach_other_owner, sensor_hand, forced) 4/4, 0.4524; final merged run: 1 REAL (`live2/pid3332804/seq040`, No-Mod, a stoppage frame: action 24, modes all 0x11, `MI+0x116C = 2`, the early-out path) + 4 SYNTHETIC = 5/5. `reach_high` was not run. No real call has yet entered the reach test or the touch chain (the `trapscan_140799060` / `14079BD10` hooks had not captured): **the touch chain `FUN_140717ED0 .. 1406E2EA0` is proven on SYNTHETIC states only; own-block 0.45**.
- Synthetic states deleted? NO: `gf-TrapContact-scratch/syn` (4 states, ~8.7 GB) still on disk: delete it.
- `gen_translated.py`: 5666 functions (3759 recorded, 1907 closure), `--check` not re-run after the final regeneration (generated by it); `ledger.py --check` current (38,429 rows, no regression, 116 off-path ports allow-listed).
- Hook request (one message, answered by Main): `trapscan_140799060` (reach test), `trapscan_14079BD10`, `trapscan_1406EDCE0` (EVERY 3000), `trapscan_1406F02F0 --when r8==2`. Landed when I last looked: `trapscan_1406EDCE0` seq40, `trapscan_1406F02F0` seq47. **Waiting captures**: the two reach-test hooks (rare); replace the synthetic reach/touch cases with them when they land (`live2/pid3332804`, `capz.py unpack`).

## 3. GF wiring (code, committed)

- `pesbodyscan.hpp/.cpp`: P-cell constants (joint block `P+0xA04` ready, positions `+0x538`, previous `+0x61C`, rotations `+0x700`, previous `+0x830`, flags `+0x2460` bits 7/9/10), the 19 collision bones in PES bone names, the driver record offsets, ball-record owner byte `+0x279C`; `LoadJointMask` reads the mask table `DAT_14351E540` behind the opcodes of `0x141EB54D0..0x141EB54DB` into `PesActorConstants::jointMask`.
- `PesWorld::RunBodyScan(slot, ext)` (rcx = P, rdx = a zeroed output record in the window's stack) and `PesWorld::WriteJointBlock`.
- `RunPesBodyScans(Match&)` (teamAIcontroller.cpp), called right after `RunPesTrapTicks`: for each man in the Trap action it writes the joint block from GF's skeleton (`BoneFramesOf`: node derived position / rotation of the 19 bones, GF axes (x, y, z up) -> PES axes (x, z, y), quaternion (x, y, z, w) -> (-x, -z, -y, w)), runs the scan, and `GrantPesTouch` when the record asks for a touch.
- `Humanoid::Process` (humanoid.cpp): on a PES trap clip (`match->PesLoop() && OnPesTrap()`) the touch fires when `Match::TakePesTouch(slot)` hands over a grant, not at `touchframe`; GF's distance and height gates are skipped on that path. `Match::PesTouchesTaken(slot)` counts grants for the tests.
- **Stand-ins (each carries a `ponytail:` naming the PES site that removes it)**:
  1. The joint block is GF's last-posed skeleton (the anime layer's `FUN_141EB49B0` is not run; AnimeLayer's job) with GF's W*Rs joint frames, which differ from PES's F_b by 1.5 degrees (leg), 5.8 (foot), ~22 (forearm, hand) (pes-re-collision.md 4).
  2. The ball owner byte `ball+0x279C` is written 0xFF (none): its PES writers (`0x140871A5C`, `0x1411944F6`) are untranslated and its meaning for a pass in flight is undecoded. The first stand-in (PES's holder) refused 28 of 28 reach tests in the first diagnostic run (owner = another man every time): wrong.
  3. The ball's velocity after the touch is still GF's `GetTrapVector` (the scan's record `+0x9C` goes to `FUN_140870A40`, untranslated).
  4. No physical contact (the ph solver is not run): the body-vs-body and sensor blocks see `P+0x3AA0 = P+0x3AA8 = 0` and are skipped, which is what a trap does anyway (section 1.4); other actions' sensor touches are not reached.

## 4. What the first run showed (CONFIRMED, probe `[tcscan]`, seed 7 pes-loop, 2 min, scratch diagnostics removed before commit)

- Tree: main + WhistleBall-less + the scan (before the merge), survey: **empty** (exit 79 only because the survey flag always exits 79; the target list is empty). Hash `45990e67bf7af287` (main `17941efc16b4a6a5`); `[idle] 4 episodes, stalls 4` (main: 0), `[pass-fail] ... trap 0-0` (main 1 failed trap). Four stall episodes: restart takers with a man in a trap clip next to a resting ball (`H5 (4): trap_0_0_000_y0_in_mirror frame 73/121`, ball 0.7 m, 0.08 m/s).
- 28 contact-pending ticks over the match (two per trap, 18.5 ms apart); each: flags `0x800200 -> 0x800000`, req 0: **refused**. In all 28 the owner byte (the holder stand-in) named another man, so the reach test was skipped before any geometry: wrong stand-in (fixed, stand-in 2).
- Geometry on those 28 (man, ball, footR, footL): the ball sits 0.2 to 1.7 m from the planted foot at the contact frame (e.g. slot 10: ball (-0.62, 0.12, -11.79), footR (-0.12, 0.33, -8.23), man (-0.65, 0, -8.48): 3.3 m away). GF's body plays the clip's authored root motion with `positionOffset = 0` (`PlayIssuedClip`, humanoidbase.cpp:532) and never applies PES's displacement (`FUN_1406E85B0`, translated by TrapTick7/8, result in P) that moves the root so the foot meets the ball. **So with PES's tight reach test (<= 0.4 m horizontal) GF's body is where PES's body is not.** The resting-ball stalls are this: the contact frame arrives, the foot is not at the ball, PES's scan (rightly) refuses.
- Named cause and owner: the pose fed to the scan lacks PES's root displacement. Fix path: feed the joint block from the translated displacement (`P` after the trap tick) or apply it to the humanoid's `positionOffset` (the same value GF's body should take: TrapTick7/8's `FUN_1406E85B0` output); until then GF's gate (1 m) is the only thing that lets a GF body touch. This belongs to AnimeLayer (pose) plus a GF body displacement wire; not examined further here.

## 5. WhistleBall merge (Main's note, 09-11-26)

`origin/WhistleBall` (6f6e7ce0) merged clean (07c70ecc). The 56-tick stall Main reports at (-8.6,-15.1) was NOT examined: the merged tree was built but `PesStall.*` was not run (budget). Candidate attribution from section 4: a taker in an idle clip + a chaser in movestep 12 is the MoveStepCell shape (no contact scan involved); a man IN a trap clip at reach with no touch is the displacement gap above.

## 6. Tests

- Written (not run): `PesStall.ATrapTouchIsTheBodyScansGrantAndNothingElse` (a man in a PES trap clip touches the ball on the tick the scan grants and on no other, and every grant touches; at least one grant in the match) and `PesStall.ATrapReachesABallAtRest` (at least one grant touches a ball at rest). They share the seed-7 match of the other `PesStall` probes. Red on origin/main (no grant counter) by construction; with the touch from the clip frame they fail on `touchesWithoutGrant`.
- **Planned mutations, not run**: (1) humanoid trigger back to `touchFrame == frameNum` (touchesWithoutGrant > 0); (2) never grant (ASSERT grants > 0); (3) take the grant but skip the touch (grantsWithoutTouch > 0); (4) swap the position axes in `BoneFramesOf` (grants collapse). Env-selected temporary mutations in one build, four parallel simslot runs, is the cheap way.
- Test binaries run: none (only `gfsim_run` smokes). Built: `gfsim_run`, `pes_restart_delay_test`.

## 7. Smoke / gate

- Baseline on main (this session, rebuilt from the same sources): pes-loop seed 7, 2 min `17941efc16b4a6a5`, `[idle] 0 episodes, stalls 0`, 29532 ticks.
- Scan tree (pre-fix, pre-merge): `45990e67bf7af287`, `[idle] 4 episodes, stalls 4`, 31407 ticks (section 4). Attribution: the scan's refusals (owner byte) removed every trap touch that GF's gate used to make; the hash move is the whole trap family.
- Post-fix smoke: launched (`smoke_new3.log`); **result not read**, see the final message.
- Drifted seeds: not censused. `GATE_TIER=branch`: **NOT RUN**. Gate not run (Main gates).

## 8. Left, in order (exact resume)

1. Read `gf-TrapContact-scratch/smoke_new3.log` (diag build, owner = none): grants vs `[tcscan]` lines. Remove the `// TC_DIAG` lines in `RunPesBodyScans` (teamAIcontroller.cpp) before committing.
2. Run `PesStall.*` in `build/tests/sim/pes_restart_delay_test` (simslot, timeout 0). If grants are 0 the displacement gap (section 4) is the blocker: wire `FUN_1406E85B0`'s displacement into the pose.
3. Mutations (section 6), `GATE_TIER=branch` on the final tip with the gate env, census of drifted seeds in the merge lane.
4. Replace SYNTHETIC reach/touch proofs with the real `trapscan_140799060` / `14079BD10` captures when they land; add a real-call proof of the touch chain.
5. Translate `FUN_140870A40`/`FUN_140874860` (the ball's kick application) to take the scan's `+0x9C` vector instead of `GetTrapVector`; decode `ball+0x279C`.
6. Delete `gf-TrapContact-scratch` (syn states 8.7 GB, dec/, logs) once archived.
