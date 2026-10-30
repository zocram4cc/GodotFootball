# CutAnchor3: goal event position `0x140476C50` and radius `0x140474CF0` translated and proven (7/7 snapshot replays each); `scene+0x60` map decoded (not ported); GF feed and frames NOT done (30-10-26)

Branch `CutsceneSelect` (worktree `gf-CutsceneSelect`), pushed heads (each confirmed by `git ls-remote origin CutsceneSelect`): `064c7c09` (merge of `origin/main`, pushed first), `a45aaac8` (translation + records + layer). PES captures are PES-derived: scratch only (`gf-CutAnchor3-scratch/`), never committed. Labels: CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 0. Status against the ticket

| item | result |
|---|---|
| 1. merge `origin/main`, push | DONE (`064c7c09`). |
| 2a. translate `0x140476C50` (T = A0+0x30) and `0x140474CF0` (radius A0+0xC) and prove them on the goal captures | **DONE**: both translated, each 7/7 synthetic calls from the 7 goal snapshots bit-exact against PES's bytes (section 1); PES's bytes reproduce A0's own values on 5 of 7 captures for each (section 2; the 2 kind-3 goals differ, cause in section 2). |
| 2b. feed them from GF state, delete the two stand-ins | **NOT DONE** (section 5). The stand-ins stay (`kSceneRunRadius`, `f.target = event`), their `ponytail:` comments still name the right VAs. |
| 3. `scene+0x60` waypoint filler | **DECODED, NOT PORTED** (section 3): who fills it, what the values are, 9-capture dump. Port needs the cut actor's animation root motion. |
| 4. frames | **NOT TAKEN**: nothing in GF's camera path changed this session (no feed), so no new frame would show anything; the CutAnchor2 frames (Godot group at the goal then roof; PES scorer tight) still describe GF. No claim about framing. |
| 5. fast tier | interim line at the end. |

## 1. Translation and proof (CONFIRMED)

- Closure: `tools/dev/translate/closure.py --spec-set core --tag cutanchor3 --subsystem cutscene 0x140476C50 0x140474CF0` = 137 members, 16 not yet in the layer (`0x140476C50`, `0x140474CF0`, `0x140476010`, `0x140476190`, `0x1404B6ED0`, `0x1404B77D0`, `0x141F5EC60`, `0x140A73E30`, `0x1404B7CF0`, `0x1404BAE20`, `0x141F53F20`, `0x141F5D770`, `0x141F61CC0`, `0x1415A17D4`, `0x141F5DE00`, `0x141F5DC50`); set file `specgen/sets/closure-cutanchor3.txt`, 26 new specs in `specs/core/` (I deleted the generated `1415A1720.json` again: a stand-in callee that makes `gen_translated` refuse, as in CutAnchor).
- **Translator fix (needed by `0x141F61CC0`)**: `0x141F5EC60` (the per-cut radius term) ends in `jmp [rax+0x18]` (virtual `0x141F616F0`); `xlate.py` emitted `__builtin_trap(); /* UNRESOLVED JMP qword ptr [rax + 0x18] */`, and the first replay died with SIGILL in `pes_fn_141F61CC0` (gdb backtrace: `141F61CC0 <- 141F5EC60 <- 140476190 <- 140476C50`). `xlate.py` now reads the slot and looks the target up in the dispatch table like an indirect CALL (abort naming the VA if untranslated), then returns. Test `tools/dev/translate/test_vtable_tail_jump.py` (2 tests): red on `origin/main`'s `xlate.py` (the `UNRESOLVED` assertion fails), green here. Mutations, each run: `[` test widened to `True` -> `test_jmp_register_stays_loud` KILLED; the `pes_dispatch_missing` line removed -> `test_jmp_through_memory_is_a_dispatched_tail_call` KILLED; `xlate.py` reverted to `origin/main` -> KILLED. Not mutation-tested: the trailing `return;`.
- Synthetic calls: the two functions have no rendezvous hook, so `tools/dev/cutscene/goal_event_calls.py` (committed) builds a recorded-call snapshot folder from each full goal snapshot: A0 = rcx of the anchor call (`0x140474F00`), ctx = its rdx (the DemoInfo builder's `param_2`; CONFIRMED: `0x140478190(A0, ctx, ...)` passes the same `param_2` to `0x140476C50`; DemoInfo = A0 - 0x1E8, cut count at DemoInfo+0xC, cut ids at DemoInfo+0x10+0x1C*i, CONFIRMED on `pid132854/seq2152`: `[0x3011b, 0x30005, 0x300a3, 0x30132]`, radius 28.7695). Event position: rdx = scratch in the caller's dead stack (`rsp+0x400`), mem.bin symlinked. Radius: the `std::vector<u32>` of cut ids is written into the same dead stack of a copy of mem.bin (the capture is never written).
- Replay: `replay_verdicts.py --hook 0x140476C50 / 0x140474CF0` over those folders (`frame_trace.py recorded` + `snapdiff.py --hooks hooks.cfg --spec-dir specs/core`). Records in `results_snapdiff_cutanchor3.json`, two `proof: snapdiff` records in `results_core.json`.

| function | captures (live2) | result | own-block coverage (best single run) |
|---|---|---|---|
| `0x140476C50` event position | `pid2621613` (regs seq1532, memory seq1531); `pid132854/seq2152, 2962, 3783, 4195, 4721, 5566` | **7/7 PROVEN**, 0 excluded, 0 mismatches | 25/61 = 0.4098 |
| `0x140474CF0` radius | same 7 | **7/7 PROVEN**, 0 excluded, 0 mismatches | 28/214 = 0.1308 |

- Layer regenerated (`gen_translated.py`): wrote 4652 functions (3687 recorded, 965 closure); same five held out as before (`1403BB670`, `1403C55D0`, `140554700`, `1405549C0`, `14202BA30`). Seven existing bodies changed (callee `0x14159FA0C` and the vtable tail jumps are now translated instead of aborting): recorded `0x140A6FF50` (88 cases bit-exact), `0x141FF52B0` (64), `0x141B50460` (64), re-run with `driver.py --out-dir ... --only` after the regen (all bit-exact); closure members `141B52380`, `141B55850`, `141FFB670`, `141FFB690` ride on their recorded callers. `gen_translated.py --check`: see the last line of section 6.

## 2. Oracle: PES's bytes against what A0 holds (CONFIRMED, `goal_event_calls.py --check-only --oracle-cfg`)

PES's own bytes (unicorn) on the synthetic call, output against A0+0x30 / A0+0xC in the same snapshot:

| capture | event position vs A0+0x30 | radius vs A0+0xC |
|---|---|---|
| seq2152, seq2962, seq4195 | equal | equal (28.7695, 21.7332, 23.2892) |
| seq5566, `pid2621613` seq1532 | **z off by 0.536** (31.524 vs 32.06): equal after rewinding the A0 RNG one draw | equal (23.2134, 13.5462) |
| seq3783, seq4721 (kind-3 rows, scene 0x300d3...) | (60.06, 24.06) / (60.06, 14.06) vs A0's (55.06, 20.92) / (55.06, 20.03) | 0.0 vs 5.2 / 6.347 |

- The default arm (`0x140476190`) draws once from the RNG at A0+0x108 (`0x140A92530`, LCG `s = s * -0x2D22B2E3 + 0x4513 & 0x7FFFFFFF`, state at A0+0x114): the snapshot holds the post-draw state, so a replay from it picks the other jitter branch on 2 of 7 captures. Rewinding one draw (inverse of the LCG, scratch `rewind.py`) reproduces A0 exactly on both (rewind 2..4 also match, the jitter has two values; rewind 0 does not). INF: the call consumed exactly one draw.
- Kind-3 goals (row byte 0 = 3, `0x1404B77D0`): A0+0x30 and A0+0xC are not what `0x140478190` wrote. INF (decompile of `0x140471A80`, caller `0x14047B460`): that function recomputes `A0+0xC` (`0x140474CF0`, three call sites), `A0+0x30` (`0x140476C50(A0, &out, ctx, 0)`) and rebuilds the cut list on a later event; the two captured kind-3 goals carry its result. Not ported, not captured (no hook on it). The translation itself replays bit-exact on those snapshots (section 1).
- What the two functions read (unicorn read hook over seq2152, scratch `trace_2152.txt`): event position = A0 `+8` (scene id), `+0xBC`, `+0x24`, `+0x48/+0x50`, `+0xC0`, RNG `+0x108..+0x114`; ctx `+0x18` (and `+0xE0` when MI+0x15A5 is set); MI (`G+0x270`) `+0x15A5`, `+0x15C8`; the scorer's man record (`+0x554`, `+0x55C..`, `+0x574..`), run object flags (`0x140A73E30`), field info (`G+0x2AA8`: `+0..+0x20`), DB scene rows. A0's defaults come from `0x140478090` (A0+0xBC = 8, +0xC0 = 8, +0x24 = 2, +0xC8 = 9 ...), which is translated; +0x20, +0xBC (1 on the kind-3 goal) and +0xC4 are written by the builder's callers (`0x140446B20`, `0x140471A80`). Radius = `0x141F5EC60(cut)` per cut whose DB flag (`0x140A7C800`, bit 5/7/0xB/0xC/0xD of the row dword at row+0xC, stride 0x38) is set: it expands the cut through the FixDemo list builder `0x141F616F0` and sums step-node floats `+0x44` (`0x141F61660`, stride 0x1E0, times `[0x14256AE18]`).

## 3. `scene+0x60` waypoint map (CONFIRMED by decompile + a 7-capture dump; NOT ported)

- **Fill**: `0x141F58160` and `0x141F58440` (called from `0x141F577B0` at `0x141F57804` / `0x141F57833`) build a 0x578-byte actor object (`0x1403FB410` alloc, ctor `0x141F7D010(obj, slot)`, setup `0x141F7DE00`) and store it in the map: `map[key = param_2] = obj`, node tag `DAT_142BA5BE0`, via `0x141F53D00` + `0x141F54270` (`obj` at node `+0x28`, key u32 at `+0x20`). So the map is NOT keyed by the step's cut id as CutAnchor2 recorded: **the key is the pitch slot 0..21** (the dump of 22 nodes, keys 0..21), the step's first layer id (`+0x1D0` list head, short at `+0x1C`) selects the slot.
- **Value**: obj `+8..+0x10` = the actor's current position, `+0x14` rotY, `+0x18..` = a copy of `+8` taken when the animation is attached (`0x141F7ECC0` copies `+8` to `+0x18`). The actor is advanced each frame by its virtual update `0x141F7E510` (-> `0x141F80070`, 1076 lines, and `0x141F7FAA0`): it is the animated cut player, so its position is the animation's root motion, not a stored waypoint.
- **Use** (`0x141F5CD00` case 2, already decoded): later steps take `obj+8` (and rotY from `0x141EA2F60(obj)`) as the stage default.
- **Data** (scratch `dump_map.py`, 7 scene-start captures; `pid132854/seq8463`, step 3): key 9 obj pos (46.393, 0, 28.250) = that step's stage default in the scene-start oracle, and its `+0x18` copy (42.943, 0, 19.896) = the previous step's (seq8456) stage default; keys 1, 7, 8, 10 hold nearby positions (47.5, 28.6), (47.1, 27.1), (47.4, 29.1), (45.8, 29.7) with rotY 17.19 (the oracle's rotY for that step), the rest `(0,0,0)`; keys 2, 5 hold a second group near (-14.6, -0.5) / (-16.1, -0.5) with other rotations. So the later stage defaults are where the celebrating actors have run to (toward the corner spot) when the next step starts.
- Why not ported: the value is the output of the actor animation system (`0x141F7E510`, `0x141F80070`, `0x141AF3100` etc.), which GF does not run for cut actors; faking it would be an unlabelled approximation. A port needs either the root-motion tracks of the actors' clips (GF's `.fdc` / animation readers) evaluated to the step boundary, or translation of `0x141F7E510`'s closure. OPEN item 3.

## 4. Frames

Not taken (section 0). The CutAnchor2 pair stays the reference: Godot starts on the group at the goal side and tilts to the roof by 4.3 s; PES (a different goal, pick unknown) stays on the scorer. Not like-for-like; no new claim. Main: a same-goal PES frame is still needed after the feed lands, and a PES goal with the cut-offset hook armed (`cut_offset_1F39CC0`, `fixdemo_scene_start_141F5CD00` armed by `goal_pick`) would give a scene-start capture for each step of one goal, which is also the oracle for the waypoint port.

## 5. Why the GF feed is not in this change (OPEN, numbered)

1. **T**: `PlaceGoal` needs an arena for `0x140476C50`: A0 (init by translated `0x140478090`, then scene id, the builder's `+0xBC/+0x20/+0xC4`), ctx (`+0x18`, `+0xE0`), MI `+0x15A5/+0x15C8`, the scorer's side/place so `0x140A62EA0` returns the slot, the run-object flags `0x140A73E30` reads, field info `+0..+0x20`, the RNG at A0+0x108 (GF's RNG goes through `pesrandom.hpp`; the A0 stream is seeded where DemoInfo is built), and for kind-3 rows the stadium record (`0x1414DAB90`, `0x141E5D4C0`). The existing arena (`fixdemo_select.cpp` `BuildWorld`) holds MI slot mode, man/run tables and field halves only; the rest is the read set above. Verify with `goal_event_calls.py --check-only` numbers (default arm, 5/7 after the one-draw rewind) as the oracle.
2. **Radius**: `0x141F5EC60` needs the cut rows' flag dwords (DB row `+0xC`, stride 0x38) and the step-node floats `+0x44`: GF's DB (`BuildDb`) lays only the key columns of each row, and `FixDemoCutRow` keeps `b0, f, b1` without a mapping to those fields. Needs the `.bin` row decode mapped to PES's 0x38-byte row (CONFIRMED layout in the capture DB: `DB+8+cat*0x20` -> rows at `+0`, groups `{start u16, count u16}`) and the cut list from the builder `0x140477BC0` (translated; its reads are in `0x140475A90`, `0x1404717C0`, `0x14047A850` ...) or GF's `Request` steps if they are proven equal to it on the captures.
3. **Waypoints**: section 3.
4. **Kind-3 goals' later recompute** (`0x140471A80`, `0x14047B460`): no capture, not ported.
5. **Foul swap** (`kOrdinaryFoulScene` -> `SanctionScene()`): unchanged, waits on FoulSanction.
6. A GF frame and the same-goal PES frame (Main arranges the PES capture).

## 6. Tests run (no gate, no sim slot; Main gates)

- `test_vtable_tail_jump.py` 2/2 (red on `origin/main`'s `xlate.py`; three mutations run, section 1).
- `driver.py --only` after the regen: `0x140A6FF50` 88 cases, `0x141FF52B0` 64, `0x141B50460` 64, all bit-exact.
- `snapdiff` 7/7 for each new function (section 1); oracle table (section 2).
- The cutscene C++ test binaries: see the final line of this section (the layer regen forces a long rebuild).
- After the second merge of `origin/main` (batch AO: extent rule, same vtable tail jump in `xlate.py`/`driver.py`; I took main's two files, ported `test_vtable_tail_jump.py` to the extent-aware synthetic image, 2/2 green) the layer was regenerated: `gen_translated --check`: 4157 functions (3691 recorded bit-exact, 466 closure) regenerate byte-identical (577 closure files of my first regen no longer exist under main's extent rule); pushed `1833fbcb` (`git ls-remote origin CutsceneSelect` confirmed). Both snapdiff replays were re-run on that tooling: **event position 7/7 PROVEN, radius 7/7 PROVEN**. The three recorded bodies re-proved by `driver.py --only` (`140A6FF50` 88, `141FF52B0` 64, `141B50460` 64) ran before this merge and were not re-run after it (OPEN for Main's gate).
- Built on the merged tree and run (`PES21_ROOT` set, `build/tests`): `pes_goal_anchor_test` 6/6, `pes_fixdemo_select_test` 6/6, `pes_fdc_test` 45/45. The other `pes_*` binaries were not run (the target build alone took 39 min under load).
- **Fast-tier interim line:** `GATE fast (interim, load makes tools/dev/gate.sh impractical): pes_goal_anchor_test 6/6, pes_fixdemo_select_test 6/6, pes_fdc_test 45/45 GREEN; test_vtable_tail_jump 2/2 GREEN; gen_translated.py --check GREEN (4157 functions byte-identical); snapdiff 7/7 + 7/7 GREEN; other pes_* binaries not run.`
