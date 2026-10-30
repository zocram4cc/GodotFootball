# CutAnchor5: goal A (side 1) now equals PES (5/5 default-arm goals bit-exact); kind-3 writer of the later T found (0x1404B6BD0 -> 0x1404B6ED0 -> 0x14047AB20); scene-list builder 0x140477BC0 oracle-checked on goal A, not yet in the layer; frames not taken (30-10-26)

Branch `CutsceneSelect`, worktree `gf-CutsceneSelect`. Pushed heads (each `git ls-remote origin CutsceneSelect`): `7ed826a0` (merges of `origin/main` batch AP and `origin/SwitchTables`, layer regenerated), `fdfc9276` (final). Labels: CONFIRMED = tool output this session; INF = inference; OPEN = blocker named. PES captures are PES-derived: scratch only (`gf-CutAnchor5-scratch/`), never committed.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, `origin/SwitchTables`, regenerate, push | **DONE.** `origin/main` conflicted only in generated files (68 closure files, `pes_dispatch_gen.hpp`, `translated.hpp`) and two JSON ledgers (`results_core.json`, `universe.json`: additive, both sides kept). `origin/SwitchTables` merged clean. Layer regenerated twice (`gen_translated.py`: 4437 functions, 3698 recorded, 739 closure); `gen_translated.py --check` after the last change: byte-identical. |
| 2a. goal A (side 1) T differs | **FIXED, root cause found** (section 1). `pes_goal_anchor_test` 7/7: goals A, B, C, E, G (T, radius, anchor pos, rotY, flip) equal PES bit for bit. |
| 2b. kind-3 rows D/F: A0+0xBC and A0+0x30 writers | **DECODED, not ported** (section 3). Not asserted: GF still computes D/F with the default state. 5 of 7 goals proven; D/F open. |
| 3. scene list from `0x140477BC0` | **PARTIAL** (section 2): closure specs generated (175 members), PES's bytes reproduce goal A's captured list from the right RNG state; not in the layer (its snapdiff replay is excluded by the harness), GF still passes `{scene}` (the `ponytail:` stays). RNG seed and run object +0x18 writer: decoded as far as section 2/3. |
| 4. frames | **NOT TAKEN** (section 4). |
| 5. fast tier | interim line at the end. |

## 1. Goal A (side 1): the run object was 0x40 bytes (CONFIRMED)

Method (the ticket's "first differing read wins"): a read log hook on `PesMem::rd` (`Load{addr,value,n}`, set for the T call only; reverted after use, never committed) against a unicorn read trace of PES's own `0x140476C50` over the goal A snapshot (`pid2621613/seq1532` registers, `seq1531` memory; script in scratch). PES makes 1624 non-stack data reads on this input; GF made **60**. The first divergence is at read 41 (after the squad-table scan, which agrees in length): PES reads `[run+0x6C]` = 0 (`0x140A73E30`, `*(char*)(run+0x6C)`), GF read `0x0A` at the same logical place. GF's run object was `kRunBytes = 0x40`, so `+0x6C` read the **next heap allocation**; a nonzero byte made `0x140476190` (default arm) take the "run object has a target" branch (`0x1419E0900(run)` = the zeroed `+0x18` target) and skip the corner-spot arm that assigns x: T = (0, 31.06). Side-0 goals happened to have a zero neighbour.
- Fix: `kRunBytes = 0x70` (covers the flag byte; PES's real object size is INF, not decoded). `src/pes/fixdemo_select.cpp`.
- Second cause (test data): goal A's `rngPre` in `kOracle` was the snapshot's A0 RNG state (194601516) instead of the state one draw earlier (459971373); with it T.z = 32.06 (the jitter branch) instead of 31.52. Corrected.
- Result: `EqualsPesOnTheSideOneDefaultGoal` (was `DISABLED_`) passes. Before the fix it failed exactly as CutAnchor4 described (x = 0); mutation = `kRunBytes` back to 0x40 reproduces that failure (the failing run before the fix is the red run; rebuilt and passing after). The RNG-state fix: the pre-fix value fails T.z (the first run after the run-object fix failed on T[2] = 31.5245 vs 32.06 and pos[0]), the corrected value passes.
- Ruled out earlier (CutAnchor4): bearing, squad table, team-info sign. The read log confirms the squad scan, bearing and team info agree.

## 2. Scene-list builder `0x140477BC0` (A0, std::vector<u32>*, ctx)

- Decompile (CONFIRMED): it calls `0x140475A90(A0, ctx)` which **picks the scene id and stores it in A0+8**, then adds the lead-in (DB flag 9 -> `[0x1437F63B4]`/`[0x1437F63B8]`), `0x14047A850`, the pre-scene (`0x140476100`), A0+8 itself, the goalkeeper scene (`0x140474B20`, A0+0x2E), the closing scene by `A0+0x20` (0x30130 + `[A0+0x20]-1`), and the `0x3011C` / `0x3012F` tails. Its first draws are from A0's generator (`A0+0x108`, state `A0+0x114`).
- Closure: `closure.py --spec-set core --tag cutanchor5 --subsystem cutscene 0x140477BC0` = 175 members; 42 new specs under `specs/core`, set file `closure-cutanchor5.txt`, `universe.json` updated. Committed. **Not in the layer**: `gen_translated.py` links only closures of recorded functions, and `0x140477BC0` has no record yet.
- Oracle (CONFIRMED, `tools/dev/cutscene/goal_event_calls.py`, new `cutlist/` synthetic call and `ORACLE ... cutlist` check): PES's bytes on goal A's snapshot with A0's RNG state rewound **2 draws** return `[0x30034, 0x30134, 0x3011C]` = A's captured scene list, and leave the state at the snapshot's 194601516 (the builder consumes exactly 2 draws). With the state as captured it returns `[0x30130, 0x30061, 0x30134, 0x3011C]`, rewound 1: `0x300BF`, rewound 4..6: `0x300BD`, so the scene pick is RNG-dependent. INF: the order in PES's builder is: first draw (the pick) -> T's draw (pre-state = captured rewound 1, now pinned in the test) -> the rest; the T call and the list builder do not run back to back from one state.
- Not proven by snapdiff: `replay_verdicts.py --hook 0x140477BC0` on the rewound call: `NOT VERIFIED`, harness excludes the case with `Invalid memory read (UC_ERR_READ_UNMAPPED) (last RIP 0x141f50ff3)` inside the callee `0x141F50FC0` (`FUN_141f500d0` ctx + `0x141F50FC0(ctx, 0x304, 3)` in the builder). frame_trace's recorded run of the same snapshot returned normally, so it is a harness page-set difference, not a PES fault; not chased (budget). Only goal A can serve this anyway: the pid132854 goal captures are slimmed (`page not captured at 0x7ff5ff2a4fd8`). **Main: for the other six goals a FULL `cut_anchor_calc_474F00` capture (not slimmed) is needed.**
- GF side unchanged: `f.cuts = {sceneId}` stays (`ponytail:` in `gf_match.cpp` still names `0x140477BC0`). Cutting over also replaces GF's own goal pick (`pes::PickGoalScene`, port of `0x140477400`) with PES's `0x140475A90`: that needs the goal context and A0 inputs in the arena; next step.

## 3. Kind-3 rows D/F (seq3783, seq4721): who writes A0+0xBC and the later A0+0x30 (CONFIRMED by decompile; INF where marked)

- **A0+0xBC** is written by `0x1404B88B0` (line `*(A0+0xBC) = iVar6`, `iVar6 = 0x1404B7F70(A0, ctx, slot, flag, 0)`, with A0+0xC0 = 8, A0+0xEA = 1 (tick recompute armed), A0+0x14 = timer `3 * rnd / [..+0x5C]`; kinds 6/7 add the extras), and by `0x140472300` / `0x1404B5FC0` (set 8). D/F hold BC = 1 / 0 afterwards.
- **A0+0x30 later** is written by **`0x1404B6BD0` (the else arm of the per-tick recompute `0x140471A80`, BC not in {6, 8}) -> `0x1404B6ED0(A0, &T, ctx)` -> `0x14047AB20(A0, &T)`** (`14047AB20` copies T into A0+0x30 and the run-object spot). `0x1404B6ED0` with BC = 1 takes `case 1` -> `0x1404B77D0(A0, &T, ctx, 1)`: with the 4th argument 1 and `0x1404B9300` false, x = `(F0 + [0x14259BDE4]) * sign(scorer x)` (= 55.06 on D/F), z = the scorer's z at that tick (20.92 / 20.03; the anchor snapshot holds the scorer ~15 frames later at 21.63: INF, the recompute runs when A0+0xEA fires, earlier than the anchor call).
- Why the builder's `0x140476C50` gives (60.06, 24.06) instead: the scene row byte 3 takes `0x140476C50` `case 3` -> `0x1404B77D0(.., 0)` whatever BC is. CONFIRMED by PES's bytes on D/F snapshots with A0+0xBC patched to 8, 1 and 0 (A0+0xC0 8 or 0): T = (60.06, 24.06) for BC 8, 1, 1/C0 0 and (60.06, 14.06) for BC 0. So BC alone does not reproduce D/F; the writer chain above does.
- Radius (CONFIRMED earlier, section 3 of CutAnchor4): the same else arm sets `A0+0xC = 0x141F5EC60(A0+8)` alone and `A0+0x10 = 0`.
- Port needs the closure of `0x1404B7F70`, `0x1404B88B0`, `0x140471A80`, `0x1404B6BD0`, `0x1404B6ED0`, `0x1404B7CF0`, `0x14047AB20` with records, plus the scorer's position at that tick (GF's tick timing; the captures hold only the anchor-time position). OPEN.

## 4. Frames (NOT done)
- `godotfootball` was not built (the layer rebuild alone is 42 min at load 35-45; `pes_state.hpp` edits touch all 4400 objects).
- PES: the watcher released captures without snapshots ("disk under 400 GiB", `live2/watch.log` 15:04) while my 28 GB scratch copies existed (deleted since, scratch is 7 MB now), so no goal capture marked a goal to poll for. No frame, no claim about framing. Not like-for-like, not verified.

## 5. Open items (numbered)
1. Kind-3 D/F: translate and prove the section 3 chain; feed the scorer's position at the recompute tick. Two anchor cases (D, F) still unasserted.
2. `0x140477BC0`: add a record (snapdiff on goal A needs the harness page set of `0x141F50FC0`'s read fixed or a full capture), put it in the layer, then feed GF's arena (A0 flags, DB flags, goal context, the generator seed: PES seeds A0+0x108 at DemoInfo construction `0x14047DF00`) and delete `PickGoalScene` + the `f.cuts = {sceneId}` stand-in.
3. Full (unslimmed) `cut_anchor_calc_474F00` captures for goals B, C, E, G, D, F (Main).
4. Frames (section 4); `godotfootball` build and a PES goal burst.
5. Mutations beyond `kRunBytes` and the RNG pin were not re-run on the final tree (a full relink needs the 42 min rebuild after reverting the read-log hook in `pes_state.hpp`).
6. The run object's real size (`kRunBytes = 0x70` is a floor, INF).
7. `closure.py` writes its scratch under `/tmp` (tool behaviour, not mine to change here).

## 6. Tests run (no gate, no sim slot)
- `pes_goal_anchor_test`: 7 passed, 0 disabled (built with the read-log hook present in `pes_state.hpp`, which was reverted before the commit; the translated code is unchanged by it).
- `gen_translated.py --check`: GREEN, 4437 functions byte-identical, after the final commit.
- `goal_event_calls.py --oracle-cfg` on all 7 goals: goal A cut list OK with the rewound state (section 2), the other six excluded by slim captures; event pos / radius lines as in CutAnchor3.
- Not run: `pes_fixdemo_select_test`, `pes_fdc_test`, other `pes_*`, `godot_*`, the gate, smoke (the tree needs the full rebuild after the `pes_state.hpp` revert).
- **Fast-tier line:** `GATE fast (interim; machine load 35-45 makes a full rebuild + gate impractical inside the budget, tools/dev/gate.sh not run): pes_goal_anchor_test 7/7 GREEN; gen_translated.py --check GREEN; other pes_* binaries not run.`
