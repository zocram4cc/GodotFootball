# CutsceneAnchor6: the else arm now runs to the end on GF's native state; the wide test reaches step 2's anchor comparison and fails on T.x, pos.x and pos.z (RED on values, not on an abort)

Branch `CutsceneAnchor`, pushed head `f6ffbad5be3c5e937eb959c7e9932afe1106144c` (confirmed by `git ls-remote origin CutsceneAnchor`). Parent `6485cd82` (CutsceneAnchor5). One commit, no force push, author zocram4cc.

Labels: CONFIRMED = run or read this round. INF = inference. OPEN = named blocker. SYNTHETIC = a call whose entry state was built by hand over seq203's memory. REAL = a call PES's own bytes made in seq203's replay (trace.json / addrs.json).

## 0. Outcome

- Target (wide test GREEN): **not met.** The test is RED on values. It no longer aborts. The else arm finishes; BC = 2 and radius = 0 pass; T and pos are compared (step 2 reached).
- CONFIRMED by run (`GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`, worktree build): 9 EXPECT failures, identical for DC 5, 100, 161:
  - `T[0]`: got -41.5502167 (the scorer's x), want -41.6211472.
  - `pos[0]`: got 0, want -41.5502167.
  - `pos[2]`: got 0, want 34.0.
  - `T[1]`, `T[2]`, `rotY`, `bc`, `radius`: pass.

## 1. The chain (what was proven and how)

Replay of PES's bytes: seq203 (recompute capture, `mem.bin` read-only) with its real trace `gf-CutsceneLine-scratch/recompute/trace203`. snapdiff non-bounded on 0x140471A80 replays PES's call and compares rax, xmm0 and every written byte with the translated layer.

| Link | Entry | Result |
|---|---|---|
| 0x140471A80 (recompute, else arm) | REAL, seq203 | re-proven on the final tree: 1/1, 0 mismatches, own-block 0.3913 |
| 0x14047C500 → 0x141F5E730 → 0x141F5E2E0 → 0x141F55110 | REAL (`addrs.json`) | inside the replay above |
| 0x14159EFF8 (array-construct loop), site 0x14159F039 | REAL (trace.json: 0x140400DF0 ×20, 0x141F55A10 ×2, 0x14086CDE0 ×28) | inside the replay above |
| 0x141F55A10 (else-arm object's constructor) | REAL (2 calls) | layer member; INDIRECT_EDGES on 0x14159EFF8 |
| 0x1419F76E0 → Q's vtable slot +0x60 = 0x141B3D6F0 → 0x141B3DFB0 | REAL (site 0x1419F787E) | INDIRECT_EDGES on 0x1419F76E0 |
| 0x141F5A8B0 (scene-object dispatcher) → 0x141F608F0 (+3) | REAL (sites 0x141F5A8C0, 8DA, 8E3, 8EC) | INDIRECT_EDGES on 0x141F5A8B0 |
| 0x141B55730 (registry find-or-create) → creators 0x14216A8A0, 0x141F53B80, 0x141F53AB0, 0x14086C830 | SYNTHETIC not-found states (`f5/s9`-`s13`) | 4/4 bit-exact (own-block 0.8, 0.8182, 0.8182, 0.8; `results_snapdiff_cutanchor8.json`, `..8b.json`) |
| 0x140421F40 (not-found callback) | SYNTHETIC | 1/1 bit-exact (own-block 1.0) |

## 2. GF-native state fixed (the ctx+0x40 style)

1. **Engine singleton** (`fixdemo_select.cpp`: `kSingletonSlotVa` 0x1437C9238, `kSingletonPageVa` 0x1437C9000, `kEngineVtableVa` 0x142AC1D38). 0x141957DF0 returns `[0x1437C9238] + 8`; 0x1419F76E0 calls `[[[that]]]+0x60`. GF had no singleton: the pointer was 0 and the dispatch went to 0. PES's seq203 holds S = 0x1505BA0, Q = S+8 = 0x15019D0, Q's vtable 0x142AC1D38 (read from the install). GF now zeroes S and Q (`Zalloc`) and sets Q's vtable to PES's VA. ponytail: S's other fields are not modelled; this arm does not read them.
2. **Find-or-create creators.** GF's registry holds only the camera names; PES found all 31 finds in seq203. GF creates MATCH/DEMO_INFO, MATCH/ENV and the camera target info on first use, which is what PES does on first use. INF: which boot-time registrations PES makes is not decoded.
3. **Array-construct helper 0x14159EFF8.** Its element constructor is a function pointer its callers pass in r9; the closure walk cannot see those. INDIRECT_EDGES lists the seq203-observed ones and the r9 constants of the layer's other callers (a static scan of the committed layer, not a runtime trace).

## 3. Checks (CONFIRMED)

- `gen_translated.py --image PES --check`: rc 0, byte-identical (5735 functions after the last regeneration; 1990 closure members).
- `tools/dev/ledger.py` regenerated and `--check`: rc 0.
- `git ls-files --others` under `src`, `tools`, `docs` (excluding `__pycache__`): empty after the commit (the new closure files are committed).
- Build: `pes_goal_anchor_test` built clean on the final tree.
- Tests: `pes_goal_anchor_test -GoalAnchorKindTwo.WideChannel*`: RED (above). The non-wide goal-anchor tests were run in CutsceneAnchor5 (22/22) and not re-run on this exact layer: **not re-verified after this round** (the layer changed; `PlaceGoal` is the path they read). Re-run: `pes_goal_anchor_test --gtest_filter=-GoalAnchorKindTwo.WideChannel*`, `pes_registry_test`, `pes_fixdemo_select_test`, `pes_camera_list_test`, `pes_proc_camera_test`.
- Mutations this round: none run (the wide test is still red; the CutsceneAnchor5 mutation on the registry slot stands).
- Gate: not run (Main gates).

## 4. Open items (numbered)

1. **T and pos differ from PES.** T.x equals the scorer's x (-41.5502) where PES holds -41.6211; pos.x and pos.z are 0 where PES holds -41.5502 and 34.0. INF: the kind-2 writer (`0x1404B6BD0` → `0x1404B6ED0` → `0x14047AB20`, the scene list builder `0x140477BC0`, the kind-2 flip `0x140478FF0`) reads state GF does not fill yet (the scene list, the scorer's zone record, or the T source). Next: dump A0+0x30, A0+0x40 and the run-object spot on the GF side after the arm, and the same fields from a replay of seq203 (the recompute's outputs), then find the first input that differs.
2. The creators are SYNTHETIC; seq203 finds all names, so no capture holds a first-use call. A capture of a registry first use would make them REAL.
3. INDIRECT_EDGES on 0x14159EFF8 and 0x141B55730 are static lists. A runtime trace of the GF test would confirm no further target is reached.
4. The worldctor 0x140441490 runs per match load (Main, seq321). GF builds the registry and the singleton once per FixDemoSelector. Not used this round.
5. Scratch `gf-CutsceneAnchor-scratch/f5/` holds the synthetic states and traces; delete with the worktree.
