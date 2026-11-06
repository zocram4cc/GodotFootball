# PES port: AnimeLayer3 (branch AnimeLayer): the player's anime constructor FUN_141EB5250 (4/4), the FoxAni registry insert FUN_141EC86D0 (3/3), project loader FUN_141AF7AB0 (3/3), FUN_141EC8D30 (1/1) and FUN_141AE87F0 (3/3) proven on REAL match-load captures; the per-player engine graph is NOT built in GF (named blocker: the fox file-load job and file layer that fill the ResManager are uncaptured and untranslated); pes-loop pose still GF's; CHECKPOINT (06-11-26)

Branch `AnimeLayer` (worktree `gf-AnimeLayer`, scratch `gf-AnimeLayer-scratch`), `origin/main` dcf1e2e9 merged. Image `pes_full.bin`, base 0x140000000. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Predecessors: `pes-port-AnimeLayer.md`, `pes-port-AnimeLayer2.md`. Every proof is `snapdiff --bounded`: PES's own bytes (unicorn, the recorded call over the capture's memory) against the translated body, every written byte plus rax/xmm0. Captures: `live2/pid2740946/` (launch 07:38, No Mod).

## 0. Status against the ticket

| item | state |
|---|---|
| 1. hooks, ONE message to Main | **DONE** (section 1). All landed: ctor x4, setup x2, loaders, inits, stage x24. Main refused the phase-3 pin (EVERY 1 pauses PES ~1,300 times/s and stalled the bot) and pulled all anime hooks after the load. A second message queued the fox file-load job hooks (section 4). |
| 2. translate and prove the FoxAni loader | **Roots proven, chain not complete** (section 2). The loader roots and everything they call on the captured paths are translated and bit-exact. The chain that makes the resource objects the ctor reads (FUN_141EA7640 and the file layer) has no capture and is not translated. |
| 3. build each player's engine graph in GF (translated loader + ctor), TLS arena | **NOT MET** (section 4). |
| 4. wire the driver phases before `RunAnimeJoints`; one-tick equality | **NOT MET** (needs 3). The pin was refused; no adjacent-tick pre-state exists. |
| 5. `GATE_TIER=branch` | **Not run**: Main has not said TeamTick landed on main (the branch merges main dcf1e2e9 but nothing in the sim calls the new functions, so no seed can drift). |

## 1. Hooks (sent to Main, one message, 05-11-26 ~07:30 local)

Window-checked lines (`untranslated_callees.hooks_for_image`, no refusals): anime_ctor_141EB5250 (EVERY 1, cap 4), anime_setup_141AFB090 (cap 2), anime_load_141EC86D0 (cap 6), anime_load_141AF7AB0 (cap 4), anime_load_141EC8D30 (cap 4), plus two NEW roots found by a static survey of the loader (anime_init_141AE77B0, anime_init_141AE87F0, cap 3), and anime_stage_1423A2260 at EVERY 1000 with no predicate. The optional phase-3 pin needed no live read: `--when 'rcx==q(q(q(q(0x1436F3DC8)+(5+0xbf)*0x28)+0x7d8+0xa08)+0x88)'` (PM = FUN_140A48A10(G,i) = `*(G+(i+0xBF)*0x28)`, P = PM+0x7D8, A = `*(P+0xA08)`, L1 = `*(A+0x88)`); Main declined it (cost). Landed captures (CONFIRMED, `live2/pid2740946`): init 77B0 seq001 seq1049; init 87F0 seq002 seq003 seq1050; registry insert seq004 seq1053 seq1058; project loader seq005 seq1054 seq1059; 8D30 seq1052; setup seq006 seq012; ctor seq1060 seq1062 seq1064 (tid 980) seq1120 (tid 984); stage x24.

## 2. Proofs on REAL calls (CONFIRMED; records in `tools/dev/translate/results_animelayer.json`, members in `closure_animelayer.json`, 758 members)

| root | what it is (decode) | calls | result |
|---|---|---|---|
| 0x141EB5250 | the player's anime constructor `FUN_141EB5250(P, "body", collision object, callback)` | 4 REAL (seq1060 1062 1064 1120) | `calls=4 replayed=4 mismatches=0 own_block=0.9032 proven=True` (replaces the 2 SYNTHETIC cases of AnimeLayer2) |
| 0x141EC86D0 | FoxAni registry insert `(registry, name, path, project)`: `strstr(name,"body")` misses: FixDemo project; hits: ten `body_anime_file%d.mtar` names, FUN_141AF7AB0, FUN_141AF4BD0(0x1125), FUN_141AF4880 x 4389, registry node | 3 REAL (seq004 seq1053 seq1058) | `calls=3 replayed=3 mismatches=0 own_block=0.9091 proven=True` (needs `GF_SNAP_BUDGET=1500000000`: the body call runs >2M instructions) |
| 0x141AF7AB0 | the project loader: looks the four skeleton resources (`body_anim_skel.ask`, `body_render_skel.ask`, `body_high_skel.ask`, `body_skel.frig`, hashes through FUN_141B035C0 in the ResManager) up and builds the fox Models R+0x20/+0x78 (FUN_141693470, FUN_1416982d0) and the MirrorL/MirrorR rig plugin | 3 REAL (seq005 seq1054 seq1059) | `calls=3 replayed=3 mismatches=0 own_block=1.0 proven=True` |
| 0x141EC8D30 | the second caller of the registry insert | 1 REAL (seq1052) | `calls=1 replayed=1 mismatches=0 own_block=0.6957 proven=True` |
| 0x141AE87F0 | the anime engine init's per-system step (under FUN_141AE77B0) | 3 REAL (seq002 seq003 seq1050) | `calls=3 replayed=3 mismatches=0 own_block=1.0 proven=True` |
| 0x141AE77B0 | the anime engine init (ResManager, type registrations, worker threads) | 2 | **NOT proven**: stops at OS exports `__intrinsic_setjmp` and `ntdll!__chkstk`, and it starts threads (`_beginthreadex`). GF does not need it: the objects are made by the per-resource steps. OPEN. |

Layer regenerated by `gen_translated.py`: 6,064 functions (3,772 recorded bit-exact, 2,292 closure); the three held-out roots (0x1403BB670, 0x1405549C0, 0x14202BA30) are the same as before this run. LEDGER.md rewritten and `ledger.py --check`: current (38,435 rows). `gen_dispatch` reports the two native heap VAs 0x1403FBC10 / 0x1403FBCB0 as members, so `mkrec` strips every `stubs.natives()` VA from the closure file (production serves them from GF's native heap).

### Findings the replays forced (all fixed, CONFIRMED by the proofs above)

- **OS import models, five new, each with a C++ mirror (`xlate.import_model_lines`) compared by the replays:** `VCRUNTIME140!strstr`, `strchr`, `strrchr` (scalar byte scans, registers as the model leaves them; `test_strstr_model.py` 23 cases); `KERNEL32!VirtualAlloc` / `VirtualFree` on a **zero OS arena** (`import_stubs.json` "os_arena": VA 0x3000000000, 4 GiB; the bump cell is `[base]`, blocks round to 4 MiB so jemalloc's chunk-aligned fast path always holds; MEM_RESET returns addr; any other form is the model's int3 and a trap in the mirror); `ntdll!RtlWakeAllConditionVariable` / `RtlWakeConditionVariable` (no waiter in a one-thread replay); `ucrtbase!_crt_atexit` (returns 0); and the `strcpy_s` model got its mirror (`"do": "strcpy_s"`). The arena is realised in `frame_trace.LazyMap` (zero blocks on first touch) and in the generated diff main (an anonymous `mmap` window in `s.mem.services`, `madvise(DONTNEED)` per call). Mutation run, rebuilt and reverted: VirtualAlloc rounding `0xFFFFFFFFFFC00000` -> `0xFFFFFFFFFF800000` makes the registry insert's replay (seq004) red (write diff).
- **`_Init_thread_notify` (0x14159F918)** is a stub in `stubs.json` (engine-noop): it wakes the MSVC thread-safe-static guard's waiters through a decoded OS pointer (`jmp rax`, 0x14159F957) that the translator cannot resolve and the dispatch cannot find; GF is single-threaded, no waiter exists.
- **`closure.py decode_row` now records `jmp [rip+x]` thunks as indirect rows** (`JMP qword ptr [slot]`), so `import_slots.py` names the slot: 0x1415A18FA (`_onexit`'s `jmp [0x142530348]`) tail-jumps to `ucrtbase!_crt_atexit`; without the row the translated thunk looked an OS address up in the dispatch table (`PES_DISPATCH_UNTRANSLATED 0x6ffffec76830`).
- **A translator unsoundness in baked read-only windows (`xlate.bake_window`):** it accepts a read when the BASE plus displacement lies in a declared table, whatever the index adds. 0x1403AE2A1 `mov eax,[rax+r8*4-0xC]` with `rax = 0x142581380` starts 12 bytes below its table, inside the last 12 bytes of the declared table 0x14257F780..0x142581380, so the window ended where the table began and the read gave 0 (the real ctor call seq1062 wrote different heap bytes: 3 of 4 passed). `ro_tables.json` now declares 0x142581374 (+0x80, the region-size reciprocals `ceil(2^21/r8)`); 4/4 pass; with the entry absent the 1062 case is the red run (found with `lockstep.py`: REP-collapsed harness trace, first control divergence at 0x1403AE2A1; the unmodified `lockstep.py` reports every REP instruction as a divergence, OPEN as a tool defect). Other functions with a window that ends exactly at a declared table's end may hide the same pattern: OPEN, not scanned.
- `snapcase.py` `INSTR_BUDGET` (2M instructions per call) is now `GF_SNAP_BUDGET` (default 2M, unchanged).

## 3. Survey: the loader chain (CONFIRMED by decompile and capture reads)

- Boot: `FUN_141AE77B0` (anime engine init; callers FUN_140406D00, FUN_141FE4A30, FUN_141FD6F10) calls `FUN_141B02800` (creates the `And::cobra::ResManager`, vtable 0x142ABB0C8, singleton `DAT_1437CEAA0`: five typed tables of 0x40-byte rows, key at +0x38, object at +0x18), registers the type loaders (FUN_141B0BA70 .. FUN_141B0CD20) and `FUN_141AE87F0`.
- The ResManager is already full when the loaders run: at seq004 it holds 344 MtFile / 5 RigFile / 11 SkelFile; at the match load (seq1059) 1,056 `fox::anim::MtFile`, 13 `RigFile`, 27 `And::cobra::SkelFile`, 4 `fox::geo::GeomFile`. They are made by the fox file-load job **FUN_141EA7640** (callers FUN_141EA7190, FUN_141EA8430): it opens the file with FUN_1414A89E0 (async handle FUN_1414A8790; state byte at +8, '\b' = done), takes the bytes (FUN_1414A8B70) and size (FUN_1414A8C70), parses, and registers through the per-type adders FUN_141B01660 / 018A0 / 01AE0 / 01D20 / 01F60 (2,892 bytes each) and FUN_141B021A0. Running on worker threads (tids 976-988).
- `FUN_141EC86D0` builds the registry node for "body" (or the FixDemo project) from the already-loaded resources; `FUN_141AF7AB0` makes the Models from the SkelFile/GeomFile objects. The constructor then clones R's Models and builds the plugins (AnimeLayer2).
- **File seam for GF:** the file layer FUN_1414A89E0 / FUN_1414A8790 (opens, async states) is the boundary where GF's VFS (`cpk_dat/common/anime/FoxAnim/Body/...`, PES's own files verbatim) replaces the OS read; the parse and registration above it stay translated PES.

## 4. Why the per-player graph is not built in GF (OPEN, exact blockers; no stand-in written)

1. **No capture and no translation of FUN_141EA7640 and the file layer.** The five hooks (anime_fox_141EA7640 / 141EA7190 / 141EA8430 / 1419AB4C0 / 1414A89E0, FIRST 1, EVERY 20) are queued by Main for the next restart. Their closure (type adders, the per-type parsers for `.mtar`, `.frig`, `.ask`, geometry) is the next slice.
2. The translated loader and ctor need a ResManager and registry in GF's window: the heap (GF's native heap serves PES's allocator, the proofs ran PES's own jemalloc translation: the native-heap comparison `snapdiff --native` was NOT run on these roots), the registry singleton `DAT_1437F4F78`, the manager `DAT_1437CEAA0`, and the Model class tables. None is built.
3. TLS arena frame for the match thread (slot +0x38 of `[0x1437067A0]`, root frame {0x14258BCA8, 0, prev, base, cursor, end, 0}) is still not written: only needed once the driver runs.
4. FUN_141AE77B0 (boot init) is not translatable as is (setjmp, `__chkstk`, threads); GF would call its per-resource steps instead.

Main suggested synthesising ResManager states from the load captures (the resource objects already sit there). That would feed the proven translated loader and ctor with PES-derived bytes that GF cannot regenerate and may not commit; it is the proof route (`synth.py` states, labelled SYNTHETIC), not a GF build. Not done: it does not advance the acceptance (a graph GF builds from the install).

## 5. Tests run, mutations

- `test_strstr_model.py` (strstr, strchr, strrchr, VirtualAlloc, VirtualFree models): 23/23 (harness Python).
- `snapdiff --bounded` x7 as in section 2 (the registry insert twice: the first run excluded the body call by the 2M guard).
- `./build/tests/pes_joint_block_test`: see section 6. `gfsim_run` built with the regenerated layer (5 min, ccache warm for unchanged objects).
- Mutations run, rebuilt and reverted: (a) VirtualAlloc granule (red, above); (b) `ro_tables.json` entry 0x142581374 absent (red: ctor seq1062, the first run). Not run: mutations of the strstr/strchr/strrchr/VirtualFree/RtlWake mirrors (their paths are exercised bit-exact by the registry insert's three replays; a mutation of each was not done).
- Rule deviation, own: `test_strstr_model.py` was extended once through a Python rewrite instead of the edit tool.

## 6. Gate, pushed state

Pushed: `git ls-remote origin AnimeLayer` = `ec6e54d309e067c1394869da675edcd2f0fff480` (earlier pushes this run: c9ba2007, e63a85f8). **GATE branch: not run** (the ticket waits for TeamTick on main; nothing in the sim calls the new functions, so no match behaviour or seed can change; the gate's drift checks `gen_translated.py --check` was not re-run after the final regeneration, which itself wrote the committed layer). Smoke: not run (no behaviour change). Tests run: `test_strstr_model.py` 23/23, `./build/tests/pes_joint_block_test` 2/2 (run directly, built against the regenerated layer), `ledger.py --check` current. Scratch (caps copies, traces, sd_*) deleted; the five captures I copied were unpacked in my own scratch only (`live2/pid2740946` is untouched, still packed). Hooks I no longer wait on: all anime_* hooks of the load (ctor, setup, loaders, inits, stage); I still wait on the five queued anime_fox_* hooks.

## 7. What is left (numbered, exact resume)

1. When Main restarts PES with the queued hooks: capture FUN_141EA7640 / 141EA7190 / 141EA8430 / 1419AB4C0 / 1414A89E0; prove them (`run_real3.sh`-style: copy and unpack the capture into scratch so auto_slim cannot repack it, `frame_trace.py recorded`, `GF_SNAP_BUDGET=1500000000 snapdiff.py --bounded --only ROOT`); list the parsers' OS models the replays stop at (iterate one export per run).
2. A GF `PesWorld` window for the ResManager / registry / Models and a VFS-backed file seam (section 3); run the proven loader and ctor per player; give the match thread the TLS arena; run FUN_141AF4650's phases before `RunAnimeJoints` in `WritePesTrapActors`; delete `BuildJointPose`.
3. One-tick oracle: adjacent-tick pre-states (the pin) or SYNTHETIC adjacent states from a load capture; then ablation, census, `GATE_TIER=branch` once TeamTick is on main.
4. `snapdiff --native` on the five roots (native heap) and a mutation per new mirror.
5. FUN_141AE77B0 (setjmp / `__chkstk` models, thread start) only if a per-resource entry proves insufficient.
6. Scan for other baked windows that end where a declared table ends (section 2, translator unsoundness).
