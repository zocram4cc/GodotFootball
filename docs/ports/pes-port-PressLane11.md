# PressLane11: who runs PES's trap-actor writer FUN_1408BE740, and when. Chain decoded; nothing landed (no captures for the match-load path)

Branch `PressLane4`, worktree `<workdrive>/gf-PressLane4`, tip **96566fe699e1998b548503e6816eadcf3e2c0933** (unchanged; nothing committed, nothing pushed). Scenario claimed: **none**. No behaviour change, no record, no test change. Scratch: `<workdrive>/gf-PressLane4-scratch/pl11/` (decompiles `dec_a.txt`, `dec_b.txt`, `dec_c.txt`; scripts `qwref.py`, `vt.py`, `ripref.py`, `readx.py`).

## 0. RIG REQUESTS (for Main: relaunch needed; hook lines are yours)

No capture of any chain function exists in `live2/` (grep of every `index.json` for the VAs: zero hits). Without match-load captures nothing here can be proven. Requested, in order of value (all through `hooks_for_image`):

1. **`0x140922D40`** (per-man setup; `rcx = PM`, `edx = team`, `r8d = idx`, `r9 = roster row`). Entry, EVERY 1, cap 22 with `--when 'q(rcx+0x7d8)==0'` (the P-allocating pass), cap 22 more with `--when 'q(rcx+0x7d8)!=0'` (the re-init pass). Must show: `PM+0x7D8` before/after, and the team/idx pairs (should walk team 0 idx 0..10, team 1 idx 0..10).
2. **`0x1408BE740`** (the writer). Entry, EVERY 1, cap 44 (same two predicates on `q(rcx+0x7d8)`). Must show: `rcx = X` (PM), `rdx` = the 0x18-byte record, `r8b` = flag; `X+0x7D8` before (0 on the first pass) and after (P); `P+0x3AC0` = `X+0x7E0`.
3. **`0x14041AAE0`** (the match-load state machine; `rcx = param`, state at `+8`). Entry, EVERY 1, cap 60, `--when 'd(rcx+8) in (6,7,8,...,0x34)'` (load states only). Must show: the state sequence, to prove the order of the 22 man steps and whether anything runs between them.
4. **`0x1406E03D0`** (`AnimePlayer` ctor, the P constructor). Entry, EVERY 1, cap 22, `--when 'q(rcx)==0'` (fresh block). Must show: the block address (its own, from the alloc), `rdx` = slot, and the vtable stores.
5. **`0x1408BE3C0`** (the gate and body-object builder called by the writer). Entry, EVERY 1, cap 22. Needed to prove the gate; its earlier proofs are on `origin/Presentation` (6/6 bit-exact per `docs/ports/README.md` ReplayRecorder4), not on Integrate.
6. **`0x140A36870`** (second writer, reached via `0x140A35B00`; in-match or not is OPEN). Entry, EVERY 1, cap 12, no predicate. Must show whether it fires in play (kick-off, restart, or goal reset) and whether `X+0x7D8` is already non-zero there.

Match-load hooks need a relaunch (hooks are read at launch). Items 1-4 are match-load; 6 may fire in the endless match now.

## 1. Verdicts

| # | Task | Verdict |
|---|---|---|
| 1 | Callers of FUN_1408BE740 and when PES runs it | **CONFIRMED.** Two direct callers: `0x140922D6C` (inside `FUN_1408BE840`, reached from `FUN_140922D40`, the per-man setup) and `0x140A368AE` (inside `FUN_140A36870`, the reset path). The per-man setup is driven by the match-load state machine `FUN_14041AAE0` (§2). |
| 2 | Decode allocation, ctor and the further calls; what GF does instead | **CONFIRMED** by decompile + disassembly (§3, §4). GF's `BindMemory` runs the X ctor only; the writer and the P allocation never run in GF (§5). |
| 3 | Port the chain where PES runs it (translated + proven) | **Not landed.** Nothing in the chain is proven (no captures). Statically translating it into the layer needs a recorded root; no root exists. §0 is the request. |
| 4 | Wire the per-slot call through 0x140925750 and delete the stand-ins | **Not landed.** The wiring needs the P allocation (task 2) first. Measurements not run: no behaviour change. |
| — | **Correction to PressLane10 §3 (CONFIRMED)** | **The "driver reads the side number as a pointer" counterexample is WRONG.** The ctor leaves `X+0x7D8 = 0` and `X+0x7E0 = 0` (§4). GF's X is null, not side. The real blocker is the missing P allocation, not a stray side number. |

## 2. The caller chain (CONFIRMED by decompile and E8/E9 site scan of the image)

- **Match-load state machine `FUN_14041AAE0`** (`dec_b.txt`). A `switch` on `*(int*)(param+8)`. It advances one state per call. States 6→0x1B call `FUN_14041A390(param, 0, k)` for team 0 (k = 0..10 one man per state; state 6 may use `(0, 0xB)` for all eleven) and `FUN_14041A390(param, 1, k)` for team 1 (states 0x11..0x1B). States 0x1F→0x34 run a second pass over the same 22 men. Other states call `FUN_1404187B0`, `FUN_140418940`, `FUN_14041CF80`, `FUN_14041CFF0`, `FUN_141E94240`, `FUN_14041A710`, `FUN_1404352E0`, `FUN_140A48A30`, `FUN_140434360`, `FUN_140402370` (not decoded). State 0x35 runs `FUN_14092BB10` over all 22.
- **Per-team loop `FUN_14041A390(param, teamSel, idx)`** (`dec_b.txt`). For each selected man it calls `FUN_140922D40(PM, team, idx, row, flag1, flag2)` (`PM = *(param + 0x10 + 8*k)`), then `FUN_1409299C0(PM)`, and `FUN_140922BC0(PM)` when the per-man dirty byte is set. `PM` is `match::Player` (X; §3).
- **Per-man setup `FUN_140922D40`** (`dec_b.txt`). `FUN_1408BE840(PM, team, idx)` (which calls `FUN_1408BE270` to build the 0x18-byte record, then `FUN_1408BE740(PM, record, flag)`), then `FUN_141EB5B30(PM+0x7D8, PM+0x624, 0)` (on P), then the position writers `FUN_1408C4D10`, `FUN_1408C4650`, `FUN_1408C4800`, `FUN_1408C4740` (decoded in `dec_c.txt`, not yet analysed).
- **Who calls the state machine (OPEN).** `FUN_14041AAE0` has no direct E8 caller. It is slot 1 of two vtables: `0x142596DC8` (installed by the ctors at `0x1404123E5`, `0x1404133DD`) and `0x142BC0100` (installed at `0x141FE3551`). Those ctors' callers are `0x141FE3524`/`0x141FE354C` (in `FUN_141FE34C0`) and `0x141FE1CCF` (in `FUN_141FE1CC0`). The owning class is OPEN. A rig capture of `0x14041AAE0` gives the caller's return address directly.
- **Second writer path.** `FUN_140A36870` calls `FUN_1408BE740(X, record, 0)`, then `FUN_141EB5B30(P, X+0x624, 0)`, `FUN_140A49C50`, `FUN_140A369E0`, and the vtable slots +0xB0/+0xC0/+0xB8 of X. Its only caller is `0x140A35B00` in the chief-spot function `0x140A35A00` (K4 in `docs/plans/pes-inventory-trackA.md`), which is called from `0x1404188D4` in `FUN_1404187FF`. Whether that is per kick-off or per restart is OPEN (hook 6).

**Timing (INF until hooks 1-3 land):** the writer runs once per man during match load (pre kick-off), in the first pass, and again in the second pass, where P already exists and only the re-init fields move. This matches `docs/re/pes-re-trap-inputs.md` §1.1 ("once per player at match setup").

## 3. The writer, decoded (CONFIRMED by decompile `dec_a.txt`; VA-cited)

`FUN_1408BE740(X = param_1, rec = param_2, flag = param_3)`, body `0x1408BE740..0x1408BE83E`:
1. `X+0x1C = *(u64*)rec`; `X+0x7CC = flag`.
2. Gate: `if (FUN_1408BE3C0(X) == 0) return 0` (`0x1408BE3C0`, body object builder: allocates the 0x410-byte body object at `X+0x7D0` and the anime objects at `X+0x938` and `X+0x940`; proven 6/6 on `origin/Presentation`, not on Integrate).
3. **If `X+0x7D8 == 0`:** `FUN_1403FB410(0x18, 0x3BC0, ...)` allocates the `AnimePlayer` block (heap service, translated on Integrate as `gameplay/pes_fn_1403FB410.cpp`), then `FUN_1406E03D0(block, X+0x18 as int)` constructs it (the slot is `X+0x18`; `P+0x2458 = slot` is written in the ctor, `FUN_1406EB670`). `X+0x7D8 = P`; a zero result returns 0.
4. `FUN_141EB5B30(P, X+0x624, 0)` (copies 12 bytes of position into the anime layer if `P+0xA08 != 0`).
5. `FUN_141EB56D0(P, X+0x61C, 0)` (the heading float into `P+0x20`; anime layer if `P+0xA08 != 0`).
6. `P+0x3AC0 = X+0x7E0` (the ctx pointer).
7. `P+0x3AD8 = (X.vtable[+0x90])(X)`.
8. `FUN_1406F0360(P, X+0x938)` (`P+0x3AE0 = X+0x938`; if non-null, `[X+0x938+0x40C] = P+0x2458` clamped to 0x15) and `FUN_1406F0350(P, X+0x940)` (`P+0x3AE8 = X+0x940`).
9. Return 1.

Decoded but not yet translated: `FUN_1406F0360` / `FUN_1406F0350` (3-line stores), `FUN_141EB5B30` (a 12-byte store plus a `P+0xA08` branch), `FUN_141EB56D0` (translated only as `restart/pes_fn_141EB56D0.cpp`; not on the Integrate layer's gameplay path).

**P constructor `FUN_1406E03D0`** (`dec_a.txt`). It writes the vtables (`0x1425E34D8` at P+0, `0x1425E3508` at P+0xAB8, `0x1425E35B0` at P+0xAC0, `0x142596D98` at P+0x3AF0 = `param_1[0x75E]`), sets `P+0x2458`, `P+0x3B18`, and calls `FUN_1406E0540`, `FUN_141EB41B0`, `FUN_1406E07D0`, `FUN_1406EB670` (decoded: `P+0xAC8 = 0x02020202`, `P+0x2458 = slot`, `P+0x3B1C = FUN_141E532D0()`), `FUN_141EB5B20` (thunk to `FUN_141EB5250`), `FUN_14041BB70`, and the import models `func_0x141E532D0`, `func_0x1415A1720` (memset). The import calls must be modelled before it can run. It is larger than the writer.

## 4. Correction: the ctor leaves X+0x7D8 = 0 (CONFIRMED by disassembly)

PressLane10 §3 said the ctor stores the side number into `X+0x7D8`. The disassembly says otherwise. `trace.txt` (from `<workdrive>/gf-PressLane4-scratch/p4/trace.txt`), `FUN_1408BD970`:

```
1408bd9a0: xor ebp, ebp
1408bd9af: mov qword ptr [rbx + 0x7d0], rbp
1408bd9b6: mov qword ptr [rbx + 0x7d8], rbp
1408bd9bd: mov qword ptr [rbx + 0x7e0], rbp
```

The `rbp` stored is zero. The translated `closure/pes_fn_1408BD970.cpp` shows the same: `gpr[5] = gpr[5] ^ gpr[5]` (line ~44) precedes the three stores. So after `pes_fn_14091E9F0` (GF's `BindMemory` loop, `pesworldrecord.cpp:105-113`), **`X+0x7D8 = 0` and `X+0x7E0 = 0` in both PES and GF.** Consequence: the driver's null-check (`cmp qword [rsi+0x7D8], 0`) sees null in GF; PES's writer allocates P (`if (param_1[0xfb] == 0)`) and makes it non-null. The P-allocation chain is therefore the blocker, not a stray side value.

## 5. The live PES values (CONFIRMED read of the capture `pid3798748/seq075`, wrapper entry, `rcx = X`)

`readx.py` on `gf-PressLane4-scratch/cap/seq075` (`call.bin`: `rcx = 0x36883DC0`, `calls = 3701`):

| Field | Value | Check |
|---|---|---|
| `X+0x18` (u32) | 17 | slot 17 (side 1, slot 6) |
| `X+0x7D8` = P | `0x3DBE8A00` | non-null (the writer ran) |
| `X+0x7E0` | `0x34E6D440` | ctx pointer, set after the ctor |
| `P+0x3AC0` | `0x34E6D440` | = `X+0x7E0` (writer step 6) |
| `P+0x3AE0` | `0x3DBE2FE0` | = `X+0x938` (writer step 8) |
| `P+0x3AE8` | `0x3DBE3DE0` | = `X+0x940` (writer step 8) |
| `P+0x2458` (u8) | 17 | = slot (ctor, `FUN_1406EB670`) |
| `P+0xA08` | `0x29EF1590` | non-null (anime layer present) |
| `P+0x20` (f32) | bits `0x43AFBC48` | = `X+0x61C` (writer step 5) |

So `X+0x7E0` (ctx) is written by PES after the ctor, not by the ctor (§4). Its writer is not decoded yet; that is the ctx blocker (PressLane9 §3.2), now at a known point: between `pes_fn_14091E9F0` and the writer.

## 6. What GF does instead (CONFIRMED by reading GF)

- `PesWorld::BindMemory` (`pesworldrecord.cpp:94-113`) runs the X ctor `pes_fn_14091E9F0` for all 2×11 slots (the PES loop's shape), then stops. It never runs the state machine, the per-man setup, the writer, or the P ctor.
- P is `PVa(slot) = kWorldWindowVa + kTrapOffset + slot * sizeof(PesTrapImage)` (`pesworldrecord.hpp:1121`): a window block inside GF's world, not a heap allocation. ~50 call sites use `PVa(slot)` / `P(slot)` directly (e.g. `pesworldrecord.cpp:403, 463-464, 809`).
- GF's stand-ins for P's ctor writes: `RunAbilityByte` / `RunEntryAim` store `p[0x2458] = slot` and the interface vtable (`pesworldrecord.cpp:791-796, 1247-1250`); `RunEntryAim` also stores `P+0x3AC0 = CtxVa(side)` (`:809`). None of these run the PES chain.
- `X+0x7D8` / `X+0x7E0` stay at 0 for every GF X, so the driver (`pes_fn_140925470`, `0x7D8` dereferenced at `0x1409256xx`) cannot run in GF.
- Driver call site in GF: `RunPesDribbleTicks` (`teamAIcontroller.cpp:3599-3643`): `RunDecisionFrame` (`:3637`) then `RunDribbleTick`. `RunDecisionFrame` = `FillPlan` + `FillTemplate` + `RunFrameBuilder` (`pesworldrecord.cpp:1055-1114`; `RunFrameBuilder` calls `pes_fn_1409F1F20`). These are the stand-ins PressLane10 named for deletion, along with `RunPress*` (`pesworldrecord.cpp:1566-1721`, `teamAIcontroller.cpp:2626`).

## 7. The port, and the decision it needs (for Main)

The writer can run where PES runs it (per-man setup, state machine order) only if the P allocation is modelled. Two options:

- **A. Heap P.** Run `FUN_1403FB410` on GF's PES heap, construct with `FUN_1406E03D0`, store the heap address in `X+0x7D8`. Then every `PVa(slot)` call site must read `*(X+0x7D8)` instead. Clean, but a cutover of ~50 sites in one change.
- **B. Window P as the allocation result.** The allocator returns `PVa(slot)` (a GF-owned zeroed block), in the same class as the graphics allocator ruling (`0x141957C50`, 23-11 night) and the heap-allocator boundary policy. `X+0x7D8 = PVa(slot)`; `FUN_1406E03D0` runs on it; the GF stand-in writes in `RunAbilityByte` / `RunEntryAim` are then subsumed. Fewer call sites change. **Recommended**, but it is a boundary-policy call (policy 6) and needs Main's OK.

Either way the next round needs: the writer proven on real calls (hooks 1-2), the P ctor proven (hook 4), the gate (hook 5), the ctx writer for `X+0x7E0` (not yet decoded: `FUN_1406B8340` writes `+0x7E0` from `gpr3`, a candidate; its callers are OPEN), and the position writers `FUN_1408C4D10/4650/4800/4740` (decoded in `dec_c.txt`, not yet proven).

## 8. Measurements

None: no behaviour change. Reference bounds on `Integrate e154ed687` (from `tasks/23-11-26.md`, unchanged here): ALiveBall 479, PesStallQuick 49. Not re-run in this lane.

## 9. Verification

- Tests run: none. Build: none. `gen_translated --check`: not run (no regen; tip unchanged).
- Commit: none. Push: none. `git ls-remote origin PressLane4` = `96566fe699e1998b548503e6816eadcf3e2c0933` (unchanged).
- Decompiles (headless `dec.sh`, Ghidra bridge down): `dec_a.txt` (writer chain, 9 functions), `dec_b.txt` (state machine, per-team loop, per-man setup, `FUN_1408BE270`), `dec_c.txt` (P ctor callees and position writers, 12 functions).
- Capture reads (read-only, from the existing `seq075`; no live PES reads this round).

## 10. Open items, numbered

1. Owner class and caller of `FUN_14041AAE0` (vtable slot 1 of `0x142596DC8`/`0x142BC0100`; hook 3 settles the caller).
2. Whether `0x140A36870` runs in play or only at load (hook 6).
3. The ctx writer for `X+0x7E0` (after the ctor, before the writer). Candidates: `FUN_1406B8340` (writes `+0x7E0` from `gpr3`).
4. Whether the match-load state machine runs anything between the 22 man steps (hook 3 predicates on the states).
5. Decision A vs B (§7). Recommend B; Main's ruling.
6. The GF stand-ins that the chain subsumes once wired: `RunDecisionFrame` / `RunFrameBuilder` (`pesworldrecord.cpp:1055-1114`), `RunPress*` (`:1566-1721`), the `P+0x3AC0` ctx store (`:809`), and the designated-man stand-in (PressLane9 cites `pressMan` in `elizacontroller.cpp`; not found by grep on this tip, OPEN).
7. The owner's ctx mismatch: PressLane2's live reads and this capture agree on `X+0x7E0` being a pointer, not the side.
8. PressLane10 §3 needs its counterexample withdrawn (§4 here). Main to archive this correction with the PressLane10 report.

## 11. Next step

Main: relaunch with hooks 1-6 (§0) and rule on A vs B (§7). Then: prove the writer and the P ctor on real calls; decode the ctx writer (item 3); wire the per-man setup in `BindMemory` in PES order; delete the stand-ins in the same change; measure ALoose, ADesignated, PesPassTarget.*, PesStallQuick, ALiveBall (20 min via simslot), and the census.

## 12. Round 2 (Main ruling: option B; relaunch 9 captures)

Status: **the writer is NOT proven and NOT wired.** Two blockers, both named below. Nothing in GF's runtime path changed.

### 12.1 Captures (relaunch 9, `live2/pid1257475/`, copied to `gf-PressLane4-scratch/pl11/caps/`; originals untouched)

- `pwrite_1408BE740`: seq001, seq004, seq007, seq011 (4 calls, ret `0x140A368B3` x3 and `0x1408BE8A0` x1). Full captures, not packed.
- `pgate_1408BE3C0`: seq002, seq005, seq008. `pset_140922D40`: seq010 (1 call).
- Entry state, all four writer calls (`readx.py`, CONFIRMED from `call.bin` and `mem.bin`): `X+0x7D8 = 0` and `X+0x7E0 = nonzero` (ctx set before the writer; so PES writes ctx before this call, not in the ctor). `X+0x1C = 0xFFFF` (the record's first qword, stored by the writer). `X+0x18` (slot) = 22, 23, 24, 0 on the four calls: **the setup order is not team-major 0..21**; slots 22..24 are the non-player objects, so GF must key P by `X+0x18`, not a loop cursor.
- `pctor_1406E03D0` was not hooked (the predicate `q(rcx)==0` did not capture; Main's note). The ctor runs inside the writer's calls, so no separate proof is needed for the writer's own closure.

### 12.2 Blocker 1: the writer's closure calls three CRT/engine imports with no model on this branch (CONFIRMED)

The gate `0x1408BE3C0` (on the writer's path) calls `toupper`, `strncmp` and `__stdio_common_vsnprintf_s` (referee-name formatter, "referee%03d"). Integrate's `tools/dev/translate/import_stubs.json` had none of them, so `frame_trace.py recorded` stopped at `unmodelled OS export ucrtbase.dll!__stdio_common_vsnprintf_s` (rip `0x6F9C35811211`, after the writer's `141EBF220`).

- Ported from `origin/Presentation` commit `d3f04aa1f` (the commit is a mixed replay-recorder landing, so only the import-model files were taken): `pes_stubs.hpp` (`pes_model_vsnprintf_s`, `pes_model_strncmp`, `pes_model_toupper`), `xlate.py` (+9 do-mapping lines), `strncmp_model.s`, `toupper_model.s`, `vsnprintf_model.s`, the three `test_*_model.py`, and three `import_stubs.json` entries. The entries' `code_hex` and `do` values were checked equal to Presentation's (script, CONFIRMED).
- Result: the import is now modelled, and the trace runs past it. **New error:** `frame_trace.py recorded seq001` stops with `unmapped data at 0x0 (size 8)` at rip `0x6F9C3653E2C6`, inside the loaded import-model code (not PES code). An 8-byte read at 0 inside a model points at the `vsnprintf_s` model's va_list read (`[args]`), so the seventh argument as the model sees it is 0. OPEN: which model faults and why (the model reads `[rsp+0x38]` at entry as `args`). Next step: decode the gate's vsnprintf call site (its stack at the call), compare the 7th-argument slot with the model's offset, and check `test_vsnprintf_model.py` on this tree.

### 12.3 Blocker 2: the writer's own allocation (option B) needs the slot, and the port is not built

- Option B as ruled: `X+0x7D8 = PVa(X+0x18)` is the allocation result, and then `pes_fn_1406E03D0` runs on it (the writer's allocation branch `if (X+0x7D8 == 0)` is then not taken). The allocator is not used for P, so the global `0x1403FB410` stays translated for every other caller. This is a GF-side step that replaces the allocator's return value for the writer's one call; it must be labelled `ponytail:` with `0x1408BE740` cited, and red-tested.
- Not built this round: the GF step (needs the slot key from `X+0x18` at BindMemory, and the ctx of `X+0x7E0`, written before the writer by a PES site not yet decoded: `FUN_1406B8340` is the candidate).
- Native services: `0x1403FB410` is a translated allocator; the native heap seam is `0x1403FBC10` (`heap_alloc`, `stubs.json`). Option B does not need a new native op.

### 12.4 Red test (written; build in progress)

- `tests/pes_press_object_test.cpp`: `SlotObjects.BindMemoryGivesEachSlotItsTrapActorPBuiltForTheSlot`. After `BindMemory`, `X+0x7D8` must equal `PVa(slot)` and `P+0x2458` must equal the slot. Install-backed (skips without `PES_EXE` and `PES21_ROOT`). Expected RED on this tip (the writer never runs in BindMemory); not yet run.

### 12.5 Closure and layer

- `closure.py --tag pslot` over the chain's 12 roots: 1,389 members (1,387 already in the universe). The full spec generation did not finish (cancelled, `bg_467`); its set file `specgen/sets/closure-pslot.txt` is partial and is NOT to be committed.
- The writer alone: `closure.py --tag pwrite --members-file` (one VA) wrote `specs/trapaction/1408BE740.json` and `specgen/sets/closure-pwrite.txt`. Note: its spec generation writes temp files under `/tmp` (the tool's own behaviour).
- No layer regen this round (the writer is not proven, so `closure_pwrite.json` does not exist).

### 12.6 Verification

- Captures read: `readx.py` on seq001/004/007/011. Decodes: `local://` nothing new beyond §2-§5.
- Tests run: none yet. Build of `pes_press_object_test`: in progress (`build_press_object.log`).
- `gen --check`: not run (no regen; the import-table change may change translations that call the three models: must be run before any push).

### 12.7 Next steps (numbered)

1. Fix the vsnprintf model fault (12.2): the seventh-argument read. Re-run `frame_trace.py recorded` on seq001..seq011; then `prove_bounded.py 0x1408BE740 ... --tag pwrite --merge` (`bg` job, `PES_EXE`, `RV_HOOKS=pl11/hooks_relaunch9.cfg`).
2. Build and run the red test `SlotObjects.BindMemoryGivesEachSlotItsTrapActorPBuiltForTheSlot`; confirm RED; then the option-B GF step (12.3) with a mutation check.
3. Run `gen --check` on the import-table change before any push.
4. Wire per-slot P in `BindMemory` in PES order (keyed by `X+0x18`), delete nothing until the writer is proven.
5. Measure ALoose, ADesignated, PesPassTarget.*, PesStallQuick, ALiveBall, census.
