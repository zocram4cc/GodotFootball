# PES port AIConcepts: concept selector 0x140579840 proven 15/15 (12 real calls), team tick 0x1404F6C30 3/3, drift step 0x1404F8C70 3/3, six concept slot roots proven; GATE fast GREEN on the phase-1 tree; GF cutover (phase 2) NOT done, blockers measured (04-11-26)

Branch `AIConcepts` (worktree `<workdrive>/gf-AIConcepts`), head `8adb7d30` (confirmed by `git ls-remote origin AIConcepts`). Image `pes_full.bin`, base `0x140000000`. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Captures: `live2/pid296049/` (tick captures seq003, seq4261, seq4479; selector captures seq004, seq4262, seq4480) and `live2/pid42407/` (new launch, hooks added at my request).

## 0. Status against the ticket

| item | state |
|---|---|
| 1. `git merge origin/main`, push | DONE: merged (`348d4af3`, no conflicts after reverting the uncommitted LEDGER regen), ledger regenerated (`8c386abe`), pushed. `gen_translated.py --check` on the merged tree: 5,087 functions byte-identical. `extern/godot-cpp` symlink restored. |
| 2. `GATE fast` on the phase-1 tree (`8c386abe`) | **`GATE fast: GREEN`** (CONFIRMED, `GATE_TIER=fast`, free pair 3/2, 100% of 2,290 tests passed; `diff_core_recorded` 7,096 s, `diff_pilot_recorded`, `diff_cutscene_recorded`, `diff_presentation_recorded`, `diff_impure_recorded` all Passed; `diff_gen_*` Passed; ledger current (38,089 rows); smoke-off rc=0, smoke-pesloop rc=0). All five recorded sets were selected in full: my phase 1 touched `stubs.json` and `import_stubs.json`, which are GLOBAL paths for `select_recorded.py`. |
| 3. Phase 2: GF cutover of `RunConcepts` and the `Team::Process` PES path | **NOT DONE.** Section 3 lists exactly what blocks it. What I did instead is the part of the blocker list that can be closed from here: the concept slots the GF ports need are now translated and proven (section 1). |
| 3. `GATE fast` again | Run on `8adb7d30` against base `8c386abe` (the phase-1 tree it was green on); result line at the end of this report. |

## 1. Proofs added this session (all `snapdiff.py --bounded`, `frame_trace.py recorded`; rax, xmm0 and every written byte vs the unicorn harness running PES's bytes)

| root | cases | result | own-block | bounded closure |
|---|---|---|---|---|
| `0x140579840` concept selector | 15: 3 from pid296049 (seq004, seq4262, seq4480) + **12 REAL calls from pid42407 taken while a concept was running** (seq8378, 36149, 54287, 54399, 55023, 67358, 67369, 122506, 122517, 122528, 123656, 123667; hook predicate: any qword of `T+0xB6E8..+0xB717` non-zero, Main's rvwatch fallback) | `calls=15 replayed=15 mismatches=0 proven=True` | 56 / 71 = 0.7887 (was 41/71 on 3 calls) | 87 (was 52): the new real calls entered the defending concepts' gates, ForeCheckPress end (`0x140574710`), LineDown start (`0x140574EA0`), the press count (`0x140574200`), the concept-0x10 execute slot `0x1405541E0` and its helpers; 2 layer-link members `0x140554410`, `0x140554620` (tail targets of `0x1405541E0`, translated, no replay entered them) |
| `0x1405747F0` ForeCheckPress start (concept 0x21, slot +0x10) | 3 SYNTHETIC | 3/3, 0 mismatches | 7/15 = 0.4667 | in the union |
| `0x140574710` ForeCheckPress end test (slot +0x18) | 3 SYNTHETIC | 3/3 | 0.5833 | " |
| `0x140575510` TransitionChase start (0x24, +0x10) | 3 SYNTHETIC + **6 REAL** (pid42407 seq018..023, hook `aic_transchase_start_575510`) | 9/9 | 0.1613 (the real calls all took the early path) | 3 |
| `0x140575EA0` RetreatBlockCreate start (0x25, +0x10) | 3 SYNTHETIC | 3/3 | 1.0 | " |
| `0x1405404A0` SideCounter end test (3, +0x18) | 3 SYNTHETIC | 3/3 | 0.2105 | " |
| `0x1405415B0` SideCounter execute (3, +0x28) | 3 SYNTHETIC | 3/3 | 0.3592 | 48 dispatch targets |

Union closure `closure-aiconcepts.txt`: **575** members (was 532); layer **5,107 functions** (3,733 recorded bit-exact, 1,374 closure; was 5,082). `ledger.py --check` current (38,092 rows).

**How the synthetic cases were made (labelled SYNTHETIC in every record; chosen, not observed).** No capture had ever entered these slots. A tick capture's own memory cannot serve them: the AI context `ctx` (`rdx`) is a stack object the tick builds before step 33, so at tick entry it is not there (first attempt: `PES's own bytes fault at [ctx]+0x18`, 0x140574861). The selector call's state is what exists mid-tick: the trace dir of each tick capture (`calls/140579840.bin` plus its call-time delta) is copied and, for each slot root, one record is written with rcx = the concept row object (`mgr + OFF[id]`, mgr = the selector's rcx), rdx = ctx and r8 = T unchanged, the same delta id. `snapdiff` then replays it exactly like a recorded call. So the state is real, the call is synthetic: the slot is invoked on the live concept object at a tick where it was not invoked. Scripts are in the scratch dir (`mk_traces.py`, not committed; the recipe is the paragraph above).

The slot offsets come from the live manager (`mgr + OFF`, offsets from `docs/re/pes-re-concept-index.md` section 0; the row's vtable slots read from `seq4479`: gate +8, start +0x10, end +0x18, +0x20, execute +0x28, interrupt +0x30). `0x140575DA0`, which the row-0x24 vtable names as its execute slot, is a 5-byte thunk (`e9 bbfdffff`, `jmp 0x140575B60`; not in `.pdata`, hence refused as a hook); the owner `0x140575B60` was already in the layer (gameplay set), and Main hooked it (`aic_transchase_exec_575B60`, one real capture so far, not replayed).

## 2. The concept table, measured (CONFIRMED: `seq4479` manager, 46 rows)

Concept objects are embedded in the manager `H+8`, 48 row slots (ids 0x14 and 0x2F are NULL arms of `0x140579E10`). Each row is an image vtable pointer (`0x1425C3D00..0x1425C4840`) plus a few fields (`+8` start tick, `+0x10` id byte). Across the 46 rows there are **157 distinct slot targets** (gate/start/end/+0x20/execute, excluding the two stubs `0x141863350` false and `0x141794410` empty), of which **65 were not in the layer** when I measured. For the six concepts GF ports (ids 3, 0x21, 0x22, 0x23, 0x24, 0x25) seven slot functions were missing; they are the six roots above plus the thunk. The static closure of those 7 is 181 functions, 149 already in the layer, 32 to translate; the bounded closures built by the replays are much smaller (3 to 48). The 40 other rows (the 20 attacking patterns `0x1405465B0..0x140572000`, the containment concepts, SealOff) stay loud aborts in the layer.

## 3. What blocks the GF cutover (OPEN, each measured)

`TeamAIController::RunConcepts` (`teamAIcontroller.cpp:2210`) is the GF hand port of exactly the selector I decompiled (enable skip, possession/in-play rule, one attacking concept at a time, gate, start, end test after `endTestAfter_s`, execute, stop). Replacing it with `0x140579840` on `PesWorld` needs:

1. **T in PES layout, and bigger.** `kSideRecordBytes = 0xA2A0` (`pesworldrecord.hpp:297`); the selector reads `T+0xB6E8` (48 running flags), `T+0xB718` (per-concept byte), `T+0xB748` and `T+0x255`, and the concept stop `FUN_14057A3E0` releases men through `T+0xA240` order words and the run-slot table (`T+0xA2A0`, `T+0x26C` blocks, `FUN_140A40820`). GF keeps its run slots in `pesrunslots` (plain C++), not in the window. This is stage S5 of `pes-re-integration.md` section 4 (T words into the record) and is not done; the selector and every concept slot read these bytes.
2. **The concept manager object.** No translated constructor exists. It is built by `0x1404F0530` (calls `0x1404F07A0` at `+8`; the H constructor `0x14041E4D0` calls it at `0x14041E647`); 55-member static closure. I built its synthetic state (`synth_ctor.py` over `seq4479`, rcx = the live object `0x39BF62C0`) and ran PES's bytes: the run stops on **unmodelled OS imports**, first `kernel32!QueryPerformanceCounter` (a model exists in `import_stubs.json`; the first run did not load it) and then `ntdll.dll!__chkstk` (no model: a Wine function, not the image's `0x14159FB70`). Adding the model is a `code_hex` entry in `import_stubs.json` plus a translator mirror in `xlate.import_model_lines` (as for `memset`, phase 1) and, as that file is GLOBAL, another full recorded re-check. Not started: it would have forced a second full gate. After it the manager still has to be run on an empty window (the DribbleProposal report found the same constructor-on-empty-window abort: a virtual call through a slot that reads 0 until the install's vtable span is bound as an image table window).
3. **The registry.** The selector asks `FUN_141E5D4C0(ctx+0x10, 0xF)` (stop flag at `+0x68`) and `(…, 0xC4)` (the subConcept enable list at `+8+4·id`, today read from the VFS `subConcept.o` into `TeamConceptConstants.enabled`). GF has no registry object; its asset registry (`0x1437F4808`, 234-slot table) is "uncaptured" per the integration doc.
4. **The concept bodies read PES state GF does not hold.** The proofs above run on captured memory. In GF's window the bodies would read `MA` nearest lists (`MA+0x22E4C`, writer OPEN), `T+0xA508`, the man/slot tables for MATCH_UP, the carrier BP, the formation slot lines: each GF input that is a stand-in today (`DefenceInput`, `ChaseInput`, `CounterInput`, `teamAIcontroller.cpp:2259-2332`) would have to be written into PES's records by a named `// ponytail:` writer. The six GF bodies then go (`pesteamconcepts.cpp`, 1,014 lines, `pes_team_concepts_test.cpp`, 533), together with `conceptRunning`, `conceptStart_ms`, `conceptOrders`, `chaseChaser`, `counterRunner` and every reader of them in the controller (the shape code reads `conceptRunning` at `teamAIcontroller.cpp:577-583`).
5. **Tick root.** The 48-step tick (`pes-re-teamtick.md` 2.2) reads and writes T, MA, ctx, E, the run manager, the carrier BP and the restart records; GF's `Team::Process` fills about half (`pes-re-integration.md` section 3). Phase 1's proof is bit-exact on three captured states only; unreached branches (mode dispatcher `0x140538050` modes 1-6, the restart branch `MI+0x1775 == 0x17`) abort loudly. A GF drive of it is gated on 1 to 4.
6. **Why I did not swap GF's bodies in as native vtable stand-ins** (the only way to cut over the selector alone): the selector would then call GF-native gate/start/end/execute through private VAs, which makes a throw-away layer between two things that must both go, needs GF state reachable from inside a translated call (a global), and still needs items 1 and 3. I judged it worse than the blocker list.

Red-first test, mutations, smokes explained by ablation, seed re-staging by census: none run, because no GF behaviour changed (the branch's diff vs origin/main is tools, records, the layer and docs). The oracle pattern for the eventual test is `tests/pes_trap_oracle_test.cpp` (oracle dirs from `trap_oracle.py`; the selector's live-in set per capture is already measured: seq004 707 heap bytes in 52 ranges, seq4262 94, seq4480 860 in 65).

## 4. Verification run (CONFIRMED)

- `GATE fast: GREEN` on `8c386abe` (above).
- `snapdiff --bounded`: selector 15/15, six slot roots 3/3, 3/3, 9/9, 3/3, 3/3, 3/3, tick 3/3, drift step 3/3; 0 mismatches. `gen_translated.py` regenerated 5,107 functions (the three standing held-out functions unchanged); ledger regenerated and current.
- No harness tooling changed this session. No mutation checks (no behaviour code).
- Captures: the 3 pid296049 tick captures and 12 pid42407 selector captures were unpacked for the replays; auto_slim re-packs them (`capz.py pack` refused: too recent). Scratch `<workdrive>/gf-AIConcepts-scratch` (about 100 MB of traces) and the worktree `<workdrive>/gf-AIConcepts-w2` (detached, same commits) can be removed.

## 5. Open items (numbered)

1. Phase 2 (section 3), in dependency order: T into the window with the run-slot table (S5); the concept-manager constructor (import models `ntdll!__chkstk`, global re-check); registry; PES-record writers for the six bodies' inputs; then the cutover with the GF bodies deleted in the same change.
2. Real calls for the slot roots with synthetic cases only: `0x1405747F0`, `0x140574710`, `0x140575EA0` (Main's hook_plan refused them: rip-relative operand in the 14-byte hook window; the selector fallback predicate covers them only when the call happens inside a selector call), `0x1405404A0`, `0x1405415B0` (hooked, none captured yet at the time of writing).
3. Unproven layer members: `0x140554410`, `0x140554620` (tail targets), the six tail targets and `0x140509890` from phase 1.
4. `snapcase._on_read` compares `mnemonic == "ret"`; a `bnd ret` loses its return slot (only `__chkstk`, now a stub, hit it).
5. The other 40 concept rows (attacking patterns, containment, SealOff): loud aborts; the 157-target table is in section 2.

## 6. Final gate line

See the last line of this file (appended when the run ended).

**GATE fast: GREEN** on `8adb7d30` (base `8c386abe`, free pair 3/2): 100% of 2,290 tests passed, including the translated-layer drift check (5,107 functions) and the ledger check (38,092 rows); the five `diff_*_recorded` checks selected 0 recorded functions (the new records live in `results_aiconcepts.json`, which none of them re-runs, as for phase 1); smoke-off rc=0 and smoke-pesloop rc=0 (no GF behaviour changed, so no hash comparison made; Main's new baselines after batch BB are switch-off 399e180f39459c2b, pes-loop 38549be3476430fd). The tree is NOT merged with BB (7636546b); merge origin/main before the next gate.
