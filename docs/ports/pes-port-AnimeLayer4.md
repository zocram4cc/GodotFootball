# PES port: AnimeLayer4 (branch AnimeLayer): the fox file-load chain FUN_141EA7640 / 141EA7190 / 141EA8430 / 1414A89E0 and the anime engine init FUN_141AE77B0 proven bit-exact on REAL captures; GF's file seam (six CRI Fs services over the install's VFS) written and tested; per-player engine graph NOT built in GF, driver NOT wired; CHECKPOINT (06-11-26)

Branch `AnimeLayer` (worktree `gf-AnimeLayer`, scratch `gf-AnimeLayer-scratch`), `origin/main` 565e6ef0 merged. Image `pes_full.bin`, base 0x140000000. CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Predecessors: `pes-port-AnimeLayer.md`, `-2`, `-3`. Every proof is `snapdiff --bounded` (PES's own bytes in unicorn over the capture's memory vs the translated body: every written byte plus rax/xmm0).

## 0. Status against the ticket

| item | state |
|---|---|
| 1. survey and translate the fox file-load job with callers and the file layer, prove on the captures | **Roots PROVEN** (section 1). The static closure of the file layer (boot constructors, the handle's reader and decode stage) is translated, NOT proven (no capture reaches it: worker threads / boot; hooks requested, section 5). |
| 2. VFS seam | **DONE as code** (section 2): the six CRI Fs calls under the handle's reader are GF services answered by `PesVfs`; tested on a real install file. The end-to-end run (translated job loads a file through it) depends on the layer regeneration, see section 6 for its state. |
| 3. build each player's engine graph in GF | **NOT MET** (section 4). |
| 4. wire the driver phases before RunAnimeJoints, one-tick proof | **NOT MET** (needs 3). |
| 5. gate | see section 6 (pes-loop: TeamTick not on main when this ran: blocked). |

## 1. Proofs on REAL captures (CONFIRMED; records `tools/dev/translate/results_animelayer.json`, members `closure_animelayer.json`)

Hooks anime_fox_* (live2/pid3147592 and pid3125670). The replays run on a worker-thread call, so the file layer's OS imports needed models (section 3).

| root | what it is (decompile) | calls | result |
|---|---|---|---|
| 0x141EA7640 | the fox file-load job (state machine on the job object: +8 state 0 new / 1 loading / 2 done, +0xC type 1..5, +0x18 path, +0x38 handle): opens the handle (FUN_1414A89E0), polls it (FUN_1414A8CE0), parses by type (FUN_141B01660/018A0/01AE0/01D20/01F60 adders), deletes the handle | 5 REAL: pid3147592 seq002, seq004, seq006, seq008 (open path), **seq484 (calls=9281, the MATCH-LOAD call: handle done, parse, adder, delete)** | 5/5, 0 mismatches, own-block 0.5517 (seq002/004/484 run), 0.1839 (seq006/008 run) |
| 0x141EA7190 | the job maker: extension test (.mtar/.frig/.ask/.gani/.geom at 0x142B65D84.. -> type 1/3/4/2/5), job object (FUN_141EA6F70), first FUN_141EA7640 call | 4 REAL: pid3147592 seq001, seq003, seq005, **seq485 (calls=381, match load)** | 4/4, 0 mismatches, own-block 0.7526 |
| 0x141EA8430 | the job pump: walks the pending list DAT_1437F48A8, makes jobs (7190), polls them (7640), drains the finished into the ResManager (FUN_141B02840) | 6 REAL: pid3125670 seq004 seq279 seq008 seq006 seq007 seq009 | 6/6, 0 mismatches, own-block 0.2128. **All six found the list empty**: the job-walking path is covered only through the proofs of 7190 / 7640 it calls. Main declined the pump hook with a predicate (cost); a SYNTHETIC state (a pending request on the list in seq484's shape, `synth.py`) is the open item. |
| 0x1414A89E0 | the file open: FUN_1403FB410 (0x98 bytes) + the handle constructor FUN_1414A8790 | 3 REAL: pid3125670 seq001 seq003 seq508 | 3/3, 0 mismatches, own-block 0.8333 |
| 0x141AE77B0 | the anime engine init (type-loader registrations FUN_141B0BA70..FUN_141B0CD20, ResManager FUN_141B02800, per-system step FUN_141AE87F0 x2) | 1 REAL: pid2740946 seq001 (the boot call, FUN_141AE77B0(0,1,0,0,0)) | 1/1, 0 mismatches, own-block 0.5143. The second captured call (seq1049) stops at a steam_api64.dll fetch: excluded. |

Not run (same code, spare captures): 1414A89E0 seq002 seq005, 141EA7190 pid3125670 seq010.

## 2. The file layer and the VFS seam (CONFIRMED by decompile; the services by `pes_fox_world_test`)

- Layer: `FUN_1414A89E0(path, &flags)` allocs a 0x98-byte handle and runs `FUN_1414A8790` (vtable 0x14296D150, semaphore at +0x60 via CreateSemaphoreW, lock at +0x68, state byte +8). `FUN_1414A9680(h, state, flag)` is the state setter: state 2 acquires the handle's semaphore (WaitForSingleObject INFINITE) and queues the handle on request manager A (DAT_143705DC8, list +0x48, SetEvent); state 5 queues on manager B (DAT_143705DD0); 1 and 8 release the semaphore. The worker stages: `FUN_1414A9280` (state 3, the reader: FUN_1414A22E0 maps "cpk_dat/xxx/" -> "eng/" and "cpk_dat/" -> "" (table 0x1434FD4A0, script-read), then the CRI Fs API: stat 0x1415A2FD0, loader create 0x1415A6CA8, set-callback 0x1415A734C, load 0x1415A71B8, status 0x1415A70B4, destroy 0x1415A6CBC; allocates the buffer +0x40, sets size +0x48; next state 4 or 5) and `FUN_1414A8F10` (state 6/7, decode: XOR keystream and FUN_1414A8A50 inflate per the file header; calls the handle's vtable +8 callback; sets +0x48/+0x50 and state 8).
- Seam: the six CRI Fs calls are `stubs.json` kind **gf-service** (new): a direct call runs `pes_service_fs_*` (`src/pes/translated/pes_native.cpp`) over the same registers and stack (xlate.stub_lines; the harness stops loudly if a replay reaches one). They answer from `PesFileSource`, implemented by `PesFoxWorld` (`src/sim/onthepitch/pesfoxworld.*`) over the existing `PesVfs`: size, read into the handle's buffer, status 2 complete / 3 error. The normalised CRI path equals the VFS path (`common/anime/FoxAnim/Body/body_anime_file0.mtar` is what BuildAnimeChain already reads). No second VFS was written.
- `tests/pes_fox_world_test.cpp` (FoxSeam): stat of an install file equals its VFS size; missing file -1; load fills the buffer with the VFS bytes and completes; load size over buffer size stops at the buffer; missing file ends in status 3.

## 3. Import models added (tools/dev/translate/import_stubs.json; each with evidence, mirrored in xlate.import_model_lines)

RtlDeleteCriticalSection, CloseHandle, CreateSemaphoreW, InitializeCriticalSectionAndSpinCount, SetEvent, ReleaseSemaphore (return values), **WaitForSingleObject now `wait`: INFINITE returns 0, any other timeout 0x102** (the old constant 0x102 made the file handle's busy-semaphore acquire spin: the first replay sat 25 min at 0x1423D9117; `test_strstr_model.py` WaitModel x3; the pump helper's poll with 0 keeps its answer), __intrinsic_setjmp (0), __chkstk (`nop`, new do kind), _beginthreadex (a nonzero handle, no thread). `closure.py`: a bridge row's "callee" that is a label inside a function's extent is skipped (it aborted on 0x1402B04BD). `ro_tables.json`: 0x142B65D83 (the five extension strings).
Model assumptions are one thread, no contended lock, no kernel state in the compared pages.

## 4. Why the per-player graph is not built (OPEN, exact)

1. The image GF maps is a DUMP of the running game: its singletons (ResManager DAT_1437CEAA0, the file manager DAT_143704E40, managers DAT_143705DC8/DD0, the pump lists) hold that game's heap pointers. `PesFoxWorld::Boot` clears them and runs PES's constructors (14149A620, 1414A3550, 1414B08A0, 1414B0E90, 1402223C0, 140222390, then FUN_141AE77B0 as the boot calls it). Left out, named: FUN_14149AB60's CRI library init (FUN_14149ABE0) and thread starters (FUN_1414B11A0, 1414B1D90): GF has no CRI library and serves the request lists itself (`PesFoxWorld::Finish`: the reader / decode stages, then the job). Marked `ponytail:` in pesfoxworld.hpp.
2. The load lists: PES's FoxAni preload is FUN_141EC9AF0 (AnimationList: Body mtar x10, skeletons, Face/Hand/Eye) queued through FUN_141EA7A50 into the job pump. Not run in GF yet.
3. The TLS scratch-arena frame for the match thread (slot +0x38 of [0x1437067A0], root frame {0x14258BCA8, 0, prev, base, cursor, end, 0}) is not written; the driver FUN_141AF4650 is not run before RunAnimeJoints; `BuildJointPose` stays (the pes-loop pose is still GF's).
4. Synthetic one-tick proof: not built (needs 3).

## 5. Hooks (one message to Main; Main's answers)

Queued by Main in run/pending_hooks.txt for the next PES restart: boot constructors 0x14149A620, 0x1414A3550, 0x1414B08A0, 0x1414B0E90 (cap 2) and 0x141EC9AF0 (cap 1). DECLINED: the pump with a `--when` predicate (the DLL pauses PES on every call); use `synth.py` states from the real 141EA8430 / 141EA7640 captures instead (not done). Refused by the window check: 0x140222390, 0x1402223C0.

## 6. Layer, tests, gate, pushed state

- Layer regenerated by `gen_translated.py` (4 runs failed first, each ~12 min: a recorded translator bug reached through 0x140288160 / 0x1403BB670 (held out since AnimeLayer3), decode noise 0x140388760, live reads of read-only image bytes in 0x1403A7290 / 0x1403B1020 (PES heap internals) / 0x1415A1AD4 (CRI error reporter), and the extension strings 0x142B65D83, declared in `ro_tables.json`): **6,695 functions (3,777 recorded bit-exact, 2,918 closure)**; the three held-out roots of AnimeLayer3 are unchanged (0x1403BB670, 0x1405549C0, 0x14202BA30). `tools/dev/ledger.py --check`: LEDGER.md current (38,464 rows). Members not proven by any capture (translated only): the static closure of the boot constructors, the reader and decode stages, the vector-constructor callbacks 0x14149A5E0 / 0x14149A6C0 and 0x1415A1BB0 / 0x1415A1F2C.
- Tests run directly: `pes_fox_world_test` 5/5 (FoxSeam: stat, missing, read, read bounded, missing-read; the end-to-end `TheTranslatedJobLoadsAnInstallFileIntoTheResManager` is DISABLED: the survey (`GF_PES_SURVEY`) shows ONE untranslated target left, **0x1415A1F7C**, a CRT/CRI error-report helper reached once, after the boot constructors, request-manager thread models and the vector-constructor callbacks were added; its caller offset 0x1C5477F in the survey is the CRT helper chain 0x1415A1BB0). `pes_joint_block_test` 2/2. `test_strstr_model.py` 26/26 (WaitModel x3 new). Mutations run: none for the new WaitForSingleObject model or the services (OPEN: one mutation per branch: INFINITE -> 0x102 should fail WaitModel and the 7640 open-path replays; read size min() in `pes_service_fs_op_read` should fail the bounded-read test).
- Smoke / pes-loop: nothing in the sim calls the new functions, so no seed can move; pes-loop smoke blocked on TeamTick's keeper unit (not on main). GATE: `GATE_TIER=branch tools/dev/gate.sh` started on the final tip 10839d49 (log `gf-AnimeLayer-scratch/gate2.log`); its verdict is in the final message of the run (it may still be running when this report is archived).
- Pushed: `git ls-remote origin AnimeLayer` = 10839d493ced5a4ba6828ebc9395794416458e24 (earlier: dffde58f).
- Rule deviations, own: `tests/pes_fox_world_test.cpp` was extended once with a Python rewrite instead of the edit tool; `mkrec_fox.py` and the closure-member edits to `closure_animelayer.json` were scripts (generated data).

## 7. What is left (numbered, exact resume)

1. Translate 0x1415A1F7C (`closure.py --tag X --no-specs 0x1415A1F7C`, add its closure to `closure_animelayer.json` members), regenerate (~14 min), build, enable `DISABLED_TheTranslatedJobLoadsAnInstallFileIntoTheResManager`; if another untranslated target shows, `GF_PES_SURVEY=file` lists them all in one run. A `ponytail:` stand-in is NOT allowed here (rule 8): the function is translatable.
2. Run FUN_141EC9AF0 + the pump in GF so the Body mtars / skeletons fill the ResManager; run the proven registry insert FUN_141EC86D0("body") and the project loader, then the ctor FUN_141EB5250 per player; give the match thread the TLS arena frame.
3. A SYNTHETIC pump-with-work state for FUN_141EA8430.
4. Real proofs of the boot constructors and FUN_141EC9AF0 when their hooks land.
5. Wire FUN_141AF4650's phases before RunAnimeJoints, one-tick synthetic equality, ablation, census, `GATE_TIER=branch` with TeamTick on main.
