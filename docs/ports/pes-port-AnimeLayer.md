# PES port: AnimeLayer (branch AnimeLayer): PES's anime engine decoded and its per-phase driver FUN_141AF4650 and the phase-3 clip blend FUN_1423A1AC0 translated and proven (49/49 SYNTHETIC, 4/4 REAL); NOT wired: the pes-loop pose is still GF's, the engine's object graph is not built in GF's window; CHECKPOINT (05-11-26)

Branch `AnimeLayer` (worktree `gf-AnimeLayer`, scratch `gf-AnimeLayer-scratch`, 32 MB), based on `origin/KickMotion` c937ee35 (the chain `BuildAnimeChain`, the joint writer and `RunAnimeJoints` live there, not on main) merged with `origin/main` 8fa4314e. Pushed: `d222357c`, `b2716cb7` (`git ls-remote origin AnimeLayer`). Image `pes_full.bin`, base 0x140000000. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Predecessors: `pes-port-KickJoints2.md`, `pes-port-KickRoot.md`, `docs/re/pes-re-anim-blend.md`.

## 0. Status against the ticket

| item | state |
|---|---|
| 1. map the anime layer engine | **DONE as a decode** (section 1, 2, 3): the pose is made by fox's AnimControl plugin chain, run as `BodyAnimControl::Exec(phase)`. The "SetMotion + per-tick blend by weight" of the brief is two separate things: SetMotion fills a request (FUN_1406F0F70) and pushes 0x30-byte records (FUN_141AF61C0, not translated); the blend and the pose are made by phases 3, 4, 5, 0xD, 0xF of the driver. |
| 2. translate, prove bit-exact, run in the pes-loop, feed FUN_141EB49B0 | **Translated and proven (section 4); NOT run in the pes-loop, NOT fed** (section 5: GF would have to build the engine's ~34 kB object graph per player; it has the clip database and a stand-in chain only). |
| 3. GF arrays == PES's captured arrays on the kickers | **NOT MET, and not reachable from these captures** (section 3): a capture holds the pose of the previous frame and the state after that frame's sync step, so no re-run of the engine on a capture's own state reproduces the capture's arrays. Real pre-state hooks were requested and now deliver (section 6). |
| 4. renderer | unchanged (section 7). |
| 5. ablation, drifted seeds, `GATE_TIER=branch` | no GF behaviour changed (layer additions only, nothing calls them), so no ablation and no census; gate line in section 8. |

## 1. The objects (CONFIRMED: RTTI of each vtable, read from the capture memory)

P = `match::AnimePlayer`. A = `*(P+0xA08)` = `And::cobra::BodyAnimControl` (vtable 0x142ABA1A8). L1 = `*(A+0x88)` = `fox::anim::AnimControl`. L2 = `*(L1+0x10)` (state block: root vector +0, dt in 1/300 s at +0x44 (5.5556 = one tick), local-pose rotations from +0x150). D = `*(A+0x188)` = `fox::gr::Model` (world bone matrices 0x40 B at `*(D+0xE0)`, joint count `D+0xF8`). B = `*(A+0xF0)` = `AnimMovePlugin`. L1+0xE8 = `fox::anim::RigFile`. L1+0xA8 = `FileCallbackT<MtarInfo>`.

The plugin list is L1+0x30 (count L1+0x28, 8-byte pointers). Each plugin has its stage at `+0x40`; the stage runner FUN_1423A2260(L1, stage) calls vtable +0x18 of every plugin of that stage whose vtable +0x30 says enabled. Measured on a kicker (index: class, stage): 0 AnimInterpPosePlugin 1; 1 AnimMovePlugin 1; 2 AnimModifyPlugin 1; 3 PluginHumanAdjustIk 1 (HumanLegIk, HumanSlantIk, HumanLookAtIk); 4, 5 AnimSyncPlugin 0; 6 AnimConvertBoneSpacePlugin 2; 7 RagdollPlugin 2; 8 RagdollConstraintIkPlugin 2; 9 RagdollPlugin 2; 10 AnimConvertBoneSpacePlugin 2.

## 2. The phases (CONFIRMED: each phase emulated alone from the capture state, pose arrays compared)

`BodyAnimControl::Exec(phase)` = FUN_141AF4650(A, phase) (vtable slot 4; its first 14 bytes hold a branch, so no hook can sit there). It is a switch over the phase:

| phase | handler | instructions | writes |
|---|---|---|---|
| 1, 5 | FUN_1423A1B50 -> FUN_1423A2260(L1, 1) | 192 / 10,414 | phase 5: the IK/modify stage edits the joint positions `*(L1+0x50)` in place (71 dwords) |
| 2 | FUN_141B11DF0 | 52 | pose-mask words |
| 3 | FUN_1423A1AC0(L1, A+0xA0, &dt) = FUN_1423A27C0 (FUN_1423B5B30 time advance) + FUN_1423A17E0 (FUN_1423B3E40 blend + forward kinematics) | 6,983 | the joint positions `*(L1+0x50)` (77 dwords) and L2's local rotations |
| 4 | FUN_1423A1900 -> FUN_1423A1960, FUN_1423A2260(L1, 0) | 431 | stage 0: the two AnimSyncPlugins (layer weights) |
| 9, 0xB, 0xD | FUN_1423A1B60 -> stage 2 | 679 / 149 / 315 | phase 0xD writes the joint rotations `*(A+0x20)` (80 dwords) |
| 0xF | FUN_1423A2920 | 261 | the Model's world matrices `*(D+0xE0)` (241 dwords) |
| 7, 8, 0x10, 0x11 | small | <= 99 | none |

The engine runs on a worker thread whose TLS slot +0x38 (`gs:[0x58]` -> `[0x1437067A0]` -> block) holds a scratch-arena frame (FUN_1403EFDF0 pushes, FUN_1403EFE40 makes a child); the match thread's slot is empty, so phase 3 emulated on a capture faults at 0x1403EFE8C until a root frame {vtable 0x14258BCA8, 0, prev, base, cursor, end, 0} is linked. A GF run needs the same arena. One full frame reads 33.8 kB in 146 byte-groups and writes 29.8 kB in 65 (mostly that arena); the rest is the 11 plugins' parameter blocks, HumanLegIk/SlantIk/LookAtIk, RagdollPlugin/Entity, Model D, the pose buffers L1+0x50/+0x60/+0x68/+0x70/+0x78 (0x390 B), the per-layer track blocks (558 / 1,572 B groups) and slot tables.

## 3. What the captured arrays are (CONFIRMED by experiment)

FUN_141EB49B0 reads L1+0x50 / A+0x20, which hold the output of the PREVIOUS engine frame. Re-running phases 3,4,5 on the capture's own state (dt 0) moves the positions by up to 6.3e-3 and no dt makes it zero (|error| minimum about 6e-3 near dt = -0.2 in 1/300 s units): the stage-0 sync (phase 4) advances the layer weights after the blend of the same frame, so the weights stored in a capture (0.8362 / 0.1638 on seq3154) belong to the next blend. Running 6CE0 and then the phases on seq3161 against the next capture of the same player (seq3172) is ~4 ticks apart (motion 3164 at 0.1350 s, then 0.2121 s) and gives maxabs 0.177. So no capture of 141EB49B0 or 1406F6CE0 can reproduce its own arrays; a real-call proof needs the pre-state of the frame (section 6).

## 4. Translated and proven (CONFIRMED, `snapdiff --bounded`)

| root | calls | kind | own-block | result |
|---|---|---|---|---|
| 0x141AF4650 (the driver, 144 members) | 49 of 52 | SYNTHETIC: phases 1 2 3 4 5 7 8 9 0xB 0xD 0xF 0x10 0x11 x the four anime_bones kicker captures (live2/pid952648 seq3154 3160 3164 3170); rcx = A, edx = phase, a root arena frame linked at the TLS slot; PES's own bytes (unicorn) are the reference | 25/31 | `calls=52 replayed=49 mismatches=0`; 3 phase-9 states excluded by the harness ("page not captured": the arena's second page) |
| 0x1423A1AC0 (phase 3: time advance, clip blend, FK) | 4 | **REAL**: hook `anime_phase3_1423A1AC0`, live2/pid2254416 seq17147 17160 17174 17234 (the real caller returns at 0x141AFF2E6, a function other than FUN_141AF4650: OPEN, see 8) | 5/7 | `calls=4 replayed=4 mismatches=0 proven=True` |

Records: `tools/dev/translate/results_animelayer.json` (SETS `("results_animelayer.json", "kick")`), `closure_animelayer.json` (root + 144 members), set file `closure-animelayer.txt`, specs `kick/141AF4650.json` and `kick/1423A1AC0.json`. Layer regenerated: 5,576 functions (3,764 recorded, 1,812 closure), `ledger.py --check` current. Not translated: FUN_141AF61C0 (the 0x30-byte record pusher; one REAL capture, live2/pid2254416 seq17301), FUN_141AF6E40 (SetSpeed), FUN_141EB5250 (the player's anime construction), FUN_1406E5FC0 (the AnimePlayer Exec).

Translator fix found by the first replay: phase 5 (IK) died with SIGILL because FUN_141B16140 holds two switches under one `cmp esi,8; ja` and the second JMP is ~33 instructions below its guard; `xlate.py` looked back 20 and emitted `__builtin_trap() /* UNRESOLVED JMP */`. Lookback 20 -> 48 (`xlate.py` prepass). Test `tools/dev/translate/test_switch_far_guard.py` (ctest `translate_switch_far_guard_test`, 3/3; the existing `test_switch_tables.py` still OK). Mutation run, rebuilt and reverted: lookback back to 20 -> 2 of 3 fail. `gen_translated.py` regenerated the whole layer with the change: no other function's text moved.

## 5. Why the pes-loop is not fed (OPEN, the exact missing inputs)

The translated driver aborts at the first read of an object GF's window does not hold. GF has (KickRoot) the clip database, the key table, one layer and a pose slot, and (KickJoints) a pose block filled from GF's node tree. The engine needs per player: A (BodyAnimControl, 0x80..0x218), L1 (0x140 B) with its 11 plugins and their parameter blocks (HumanLegIk, SlantIk, LookAtIk, AnimInterpPose, AnimModify, AnimMove, two AnimSync, two AnimConvertBoneSpace, two RagdollPlugin + RagdollConstraintIk, RagdollEntity), L2 (0x400 B: state, local-pose rotations), pose buffers L1+0x50/0x60/0x68/0x70/0x78, the Model D with its 20-bone matrix array, the RigFile at L1+0xE8, the per-layer track blocks (the clip's tracks in the engine's own layout: 558 / 1,572 B groups; KickRoot builds the pack, not these), the slot tables (0x30E45xxx groups), the thread's arena. The constructor is FUN_141EB5250 (static closure 195 functions; virtual and resource-loading calls not counted): translating it needs captures of the constructor, which no hook has. The SetMotion side needs FUN_141AF61C0 translated (seq17301 is its one real call). Order of one frame (INF from the phase numbers): SetMotion pushes (match tick) then 1,2,3,4,5,7,8,9,0xB,0xD,0xF..0x11.

## 6. Hooks (requested and live)

`anime_phase3_1423A1AC0` (11 captured, 4 replayed), `anime_stage_1423A2260` (`d(rdx) in (1,2)`), `anime_phase4_1423A1900`, `anime_pusher_141AF61C0` (1 captured). The watcher stopped near the disk floor, so stage/phase4 captures had not landed when I stopped. With them: real proofs of the stage runner and of phase 4, and FUN_141AF61C0; for the pose proof (item 3) take a phase-3 capture, run phases 3,4,5,0xD from it and compare with the NEXT hooked frame's pre-state of the same player only if two captures are adjacent ticks; otherwise the real-call proof of each phase is the strongest statement available.

## 7. What the renderer would need

The pose GF's renderer shows is the node tree animated by GF's `Animation` (one clip, GF's smoothing). To show PES's pose it would read the engine's output arrays per player (`*(L1+0x50)` positions, `*(A+0x20)` rotations, model space) instead; both are 20 joints in rig-joint order (KickJoints notes). Not changed.

## 8. Tests, gate, left

Run: `test_switch_far_guard.py` 3/3 and `test_switch_tables.py` OK (harness Python), `gen_translated.py` (regenerated), `ledger.py --check` (current), `snapdiff --bounded` as above; the tree builds. GATE branch: see the last line of this report. Not run: ablation, census (nothing calls the new functions).

Left (numbered, exact resume):
1. Prove the stage runner FUN_1423A2260, phase 4 FUN_1423A1900 and the pusher FUN_141AF61C0 on their real captures when they land (`run_real.sh`-style: unpack at most 4, `frame_trace.py recorded --hooks <live hooks.cfg>`, `snapdiff.py --bounded --only ROOT`, pack).
2. Identify the real caller at 0x141AFF2E1 of FUN_1423A1AC0 (not FUN_141AF4650: a second AnimControl driver) and make it a root too.
3. Ask Main for a capture of the constructor FUN_141EB5250 (or build the objects of section 5 from the install by hand with `ponytail:` naming FUN_141EB5250), then run the driver per player in `WritePesTrapActors` before `RunAnimeJoints` and delete `BuildJointPose`; after that the joint block is PES's.
4. Then the item-3 oracle test and the ablation/census/gate.
5. 3 phase-9 states of the synthetic proof were excluded (arena page): regenerate them with a larger captured arena if a proven=True record is wanted.

## 9. Gate (CONFIRMED, `GATE_TIER=branch tools/dev/gate.sh`, log gf-AnimeLayer-scratch/gate.log)

**GATE branch: RED (not completed).** Branch select: 2,357 tests (412 changed paths against 8fa4314e); smoke-off rc=0 (30,213 sim ticks, `[idle] 12 episodes, stalls 0`); **smoke-pesloop rc=134: `PES_DISPATCH_UNTRANSLATED: 0x142132e20`** (the keeper unit), which is not this branch's change: the base is `origin/KickMotion`, which lacks TeamTick (KickJoints2 section 0/7 said the same: "TeamTick is NOT on main, so the keeper unit 0x142132E20 still aborts a normal pes-loop match"). The brief put the work on main, but the chain, the joint writer and `RunAnimeJoints` exist only on KickMotion, so the slice cannot be gated green until TeamTick is merged under it (landing order from Main: TeamTick, then KickMotion, then this). The gate then hit my 3,600 s command limit during the ctest phase (no test verdict). New code reached by no caller, so the expected effect on smokes is none; smoke-off hash not compared (no baseline exists for the KickMotion tree). Left item 6: merge `origin/TeamTick` (or main once it lands) into AnimeLayer, regenerate the layer (`gen_translated.py`; conflicts in `pesworldrecord.*`, `gen_translated.py` SETS, `universe.json`, `LEDGER.md` are the known ones), rerun `GATE_TIER=branch` with `timeout: 0`.
