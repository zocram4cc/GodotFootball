# PES port TackleContact: the tackle reach test, the clip solver, the tracked-part distances and the contact-chain head are proven bit-exact on live calls; the harness now replays recursive calls (03-11-26)

Branch `TackleContact` (worktree `<workdrive>/gf-TackleContact`, scratch `gf-TackleContact-scratch`). Last pushed SHA `920245e6` (confirmed `git ls-remote`), on `origin/main` `2f86b225`. Labels: CONFIRMED = a tool run this session; INF = inference; OPEN = blocker named. Image `pes_full.bin`, base `0x140000000`. Captures: `live2/pid296049/` (hooks `mot_*` of docs/re/pes-re-portmap-motion.md rows T1, C1, H1) plus the new launch `live2/pid1037286/` (Main armed `mot_tackle_reach_78A640` and `mot_clip_solver_6ECBB0`, cap 24, on my request).

## 0. Status

| item | result |
|---|---|
| Phase 1: translate and prove every root | **DONE.** 5 of 5 roots, every replayed call bit-exact (section 1). Pushed. |
| Phase 2: GF cutover (`solverOk`, `pescontact.cpp` rigid offset, `ball.cpp` touch spin) | **NOT DONE: blocked by inputs GF does not have**, named per site in section 4. No GF source changed, no stand-in retired, nothing faked. |
| gate | `GATE fast` NOT run (phase 1 builds nothing in GF). Interim line in section 5. |

## 1. The proof (CONFIRMED)

Recipe: `closure.py --members-file` (root-only specs, set `contact`), `frame_trace.py recorded` over each capture, `snapdiff.py --bounded --only <roots>` (PES's bytes in the unicorn harness against the translated bounded closure, rax, xmm0 and every written byte compared), then `closure.py --no-specs --members-file`, `gen_translated.py`.

| root | PES function | calls replayed | mismatches | own-block coverage | bounded closure |
|---|---|---|---|---|---|
| `0x14078A640` | tackle reach test (one kind row of `0x14078B550`'s loop; T1) | **4 / 4** | 0 | 0.3396 (72/212) | 33 |
| `0x1406ECBB0` | clip solver physics prediction (T1 callee; recursive) | **23 / 23** | 0 | 0.7264 | 29 |
| `0x140842BE0` | per-part tracked distance (C1; hip 3D, the rest horizontal) | **8 / 8** | 0 | 1.0 (10/10) | 4 |
| `0x140843C50` | max over the four parts (C1) | **3 / 3** | 0 | 0.6364 (28/44) | 7 |
| `0x140876020` | contact-chain head into the `0x140876360` tick (H1) | **3 / 3** | 0 | 1.0 (15/15) | 20 |

Union closure: **78 functions**, all translated, no blocked callee (`blocked.json` empty for all five). Captures: `pid296049` seq4353, 7370, 10807 (reach), seq020, 4340, 4429 (solver), seq024, 048, 1286, 041, 7242, 13815 (tracked and max), seq010, 8430, 12287 (head); `pid1037286` seq28770 (reach), seq22101, 23501, 23800, 23831, 23850, 23854, 23909, 24069, 28318, 28407, 28496, 28611 (solver; 19 calls). Every capture was already slim, so nothing needed packing.

Records: `tools/dev/translate/results_contact.json` (5 records, `proof: snapdiff`), `closure_contact.json`, `gen_translated.SETS` gained `("results_contact.json", "contact")`, `specs/contact/<root>.json` (root specs only; the other 73 members are listed in `specgen/sets/closure-contact.txt`, as for the kick chooser), `universe.json` +15 rows. `ro_tables.json` +2 (section 3). Layer: **4,851 functions (3,724 recorded bit-exact, 1,127 closure)**, regenerated twice; the first regeneration refused at `ro_tables` (section 3). `gen_translated.py --check` was byte-identical after the first regeneration (7 m 23 s); the check on the final tree is in section 5.

What the captures show (CONFIRMED, from the traces): all four reach calls return 0, so the arm of `0x14078A640` that returns 1 (the kind row solved) is **not** covered (own-block 0.34). Tackles are rare in the bot match and the hook samples one call in 2,000 (hooks.cfg `15 1 2000`); a solved-arm capture needs more reach captures from `pid1037286` or a later launch. The contact head's closure holds the tick `0x140876360`, the copier `0x140731BB0` and the `0x140873EE0` chain, but **not** the foot response `0x140874860` -> `0x140875580`/`0x140875A10`: `0x140874860` is called from the intended-touch writer `0x140870A40`, whose hook window is refused (rip-relative operand), so no capture of this hook enters it.

## 2. The harness fix: a recursive call returns to its own return address (CONFIRMED)

`0x1406ECBB0` is recursive (xref `0x1406EDBCC` inside itself). Three of the first four solver replays failed: the recorded return address of an inner call is `0x1406EDBD1`, and the harness stopped at the first arrival at the recorded return address, which is the deeper inner return (`frame_trace`: `stack not balanced: rsp 0x1a4fde60, want 0x1a4fe090`; `snapdiff`: `did not return to 0x1406edbd1`, 3 of 4 calls excluded, `proven=False`). The harness now ends a recorded call only when the return lands with `rsp == recorded rsp + 8`; an earlier arrival is stepped over and the run continues.

- `tools/dev/diffharness/run.py`: new `emu_call(uc, entry, ret, want_rsp, count)`; the install_hooks stop at the sentinel is conditional on `uc._ret_rsp` (absent = the old unconditional stop, so every other user is unchanged).
- `tools/dev/diffharness/snapcase.py` and `tools/dev/translate/frame_trace.py`: call `emu_call` and set `uc._ret_rsp = rsp + 8`.
- **`run.py` and `snapcase.py` are GLOBAL for `select_recorded.py`: landing this branch re-checks every recorded function** (Main knows; warm cache about 25 min).

Test (`test_recorded_call.py`, 13/13): `test_recursive_return_to_the_calls_own_return_address_does_not_end_the_call` (a 5-instruction recursive function recorded with `ret = CODE+11`, the recursion's own call-site continuation; the replay must run `inc rax` once and end at `rsp + 8`). Red on the old code: `stack not balanced: rsp 0x700800, want 0x700808`. `make_snapshot` gained a `ret` argument. Mutations run and reverted: M1 old `emu_start(entry, ret)` (the three-file change stashed) = the new test fails; M2 `emu_call` without the "step refused, return" guard = `test_unbalanced_return_is_an_error` **hangs** (an unmapped return address cannot be stepped), found and fixed (the guard is the first loop line after the step); the unbalanced-return test still reports `stack not balanced`.

## 3. Read-only tables the closure reads (CONFIRMED / INF)

`gen_translated` refused the first regeneration on two live reads of read-only image addresses; both are declared in `ro_tables.json` (a table read live is 0 at GF runtime):

| table | reader | extent | verdict |
|---|---|---|---|
| `0x1425EFBE0` | `0x140842BE0` site `0x140842C6D` `mov r8d,[rcx+rdi*4]`: bone index per tracked part (HIP 0, FOOT_L 3, FOOT_R 6, HEAD 10; read from the image: `00 00 00 00 03 00 00 00 06 00 00 00 0a 00 00 00`), part 0..3 (`0x140843C50` loops `iVar6 < 4`) | 0x10 | CONFIRMED |
| `0x1425E9F10` | `0x14078F6C0` site `0x14078F6DD` `movzx ebx,word [rcx+rax*2]`: `0x10, 0x14, 0x14` then the zero before the next table at +8 | 0x8 | values CONFIRMED; the index bound (speed class 0..2 from `0x14078F4F0`'s loop through `0x140791870`, `0x14078B550` param_5) is INF |

## 4. Phase 2 blockers (nothing changed in GF)

Each cutover must call the translated function on PES-layout state that GF has to supply. What each one needs, from the decompiles of the proven roots and the GF sites:

1. **T1: `solverOk` always yes (`pestackle.cpp:318`, `ChooseTackle`; header `pestackle.hpp:148`).** `0x14078B550` calls `0x14078A640(P, ap, kind, rec, speedClass, row, ...)` per candidate row and stops at the first non-zero return; the reach test reads the AnimePlayer's clip interface (`ap+0xAB8` vtable calls), the clip's HitData position (`0x140439E50`), the carrier prediction (`0x14046F230`), and calls the solver `0x1406ECBB0`, which steps a capsule over animation records. GF's `TackleSituation` carries none of these, and `Animetable` rows resolve a motion id but no clip record. OPEN: a PES-layout AnimePlayer with clip records for the tackler (HitData/Animation records of the named motion) and the carrier-prediction record in `PesWorld`. Also INF: with all four captured reach calls returning 0, enabling the real answer would turn nearly every GF tackle pick into kind 7 until the solved arm is covered; that needs more captures first (section 1).
2. **C1: rigid offset (`pescontact.cpp:260-368`, `match.cpp:2005`).** `0x140842BE0(P, rec, part)` reads the tracked-part points `P+0x508+part*0xC` (writer undecoded: the stand-in's own removal site), the bone world position through `0x141EB51A0(P, out, bone)` (the bone block `P+0x538`), the clip interface (`P+0xAB8` vtable slot 0, `PesWorld` already carries the `0xAB8` vtable in `pestrapworld.hpp`) and `MI+0x1198` (`kMiFrameOffset` exists), and returns 0 when `rec+0x51 == 0` and the frame gate holds. GF has per-bone collision centres (`parts[i].Center`) and the rigid animation offset, but not the tracked points: feeding the translated function with `tracked = bone - rigid offset` would only reproduce the rigid model through PES's per-part formula, i.e. a new stand-in that must be named. OPEN: the `P+0x508` writer (decode, then a live read of tracked versus bone points over a contact).
3. **H1: touch spin (`ball.cpp:94`, `Ball::SetRotation`).** Its own ponytail names the removal site: the foot response `0x140874860` -> `0x140875580` / `0x140875A10` under the intended-touch writer `0x140870A40`. The proven head `0x140876020` does not reach it (section 1). `0x140875580` is already recorded bit-exact in `results_core.json`; `0x140874860` and `0x140870A40` are not recorded, and `0x140870A40`'s hook window is refused. OPEN: a root that reaches `0x140874860` (a hook at `0x140874860` itself, or a harness build around the refused window) and captures of it.

## 5. Checks run

| check | result |
|---|---|
| `snapdiff.py --bounded` over all captures (twice: before and after widening) | 5 of 5 proven, 41 calls, 0 mismatches |
| `gen_translated.py --check` (first regeneration, tree `cc000f10`) | 4,851 functions byte-identical |
| `gen_translated.py --check` (final tree `920245e6`, pushed) | rc 0: 4,851 functions (3,724 recorded bit-exact, 1,127 closure) regenerate byte-identical |
| `test_recorded_call.py` 13/13, `test_closure_walk.py` 23/23, `test_select_recorded.py` 9/9, `test_image_reads.py`, `test_layer_headers.py` 2/2, `test_tail_jump_loud.py` 9/9, `test_cond_tail_call.py` 3/3 (harness Python) | OK |
| PII grep of the staged diff (`<workdrive>`, `<local>/`) | 0 hits |

**`GATE fast (interim; tools/dev/gate.sh and every GF build NOT run: phase 1 compiles nothing in GF, and the machine is loaded): gen_translated.py --check GREEN; the python tests above GREEN; snapdiff --bounded 5/5 proven. The five new TUs (pes_fn_1406ECBB0 moved from closure/ to contact/, etc.) were not compiled by the GF build; snapdiff compiled each of them in its own harness build.`** Main's landing gate re-checks everything because of the GLOBAL harness change (section 2).

## 6. Open items

1. Phase 2 for T1, C1, H1 (section 4): the three missing inputs (clip/HitData records and the carrier prediction in `PesWorld`; the `P+0x508` writer; a root that reaches `0x140874860`).
2. Reach solved-arm coverage: more `mot_tackle_reach_78A640` captures (the launch `pid1037286` armed it, cap 24; 1 landed in the first 20 minutes) until one returns 1.
3. `pes_fn_1406ECBB0.cpp` moved from the `closure` layer directory to `contact` (it is now a recorded function): `grep` over `src`, `tools`, `tests` and `CMakeLists.txt` finds no path that names the old location (callers link by function name through `pes_dispatch.hpp`); the GF build is not run here.
4. Own-block coverage of the five roots is recorded in `results_contact.json`; none is below the 0.34 of the reach test.
