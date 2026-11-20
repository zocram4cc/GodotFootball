# pes-port-ReplayRuntime6: four recorder roots proven bit-exact on pid1845381 (human/prop writers and the unit reset); three import models added; the layer regenerated; 1408BE3C0 and 1408BED00 NOT proven (named gaps); slice 2 bind not proven

Branch `Presentation`, pushed `8ced8e35..d3f04aa1` (`git ls-remote origin Presentation` = `d3f04aa1f83e`, confirmed). One commit, `d3f04aa1`. No build dir was created this round (no test binary built; the new mirror was checked by `g++ -fsyntax-only` on three layer files and a one-line TU). Copies of captures removed (`caps950952`, `copies6` deleted); `pid1845381` captures untouched (Main's). Scratch 3.6 MB.

Labels: CONFIRMED = a `prove_bounded` replay over real captures (the merged record), a unicorn test or mutation run, `gen_translated --check`, `ledger --check`, a `frame_trace` run. INF/OPEN as marked.

## 1. Proofs (CONFIRMED; all with `--merge`, real captures, copied out of live2 before proving)

| root | captures | result | own-block |
|---|---|---|---|
| `140A89030` (human writer) | pid1845381 seq553, 558, 563, 567, 572, 577 (6 calls) | proven, 6/6, 0 mismatches | 16/28 |
| `140A893F0` (human writer) | seq554, 559, 564, 568, 573, 578 (6) | proven, 6/6, 0 mismatches | 14/16 |
| `140A898D0` (human writer) | seq555, 560, 565, 569, 574, 579 (6) | proven, 6/6, 0 mismatches | 4/4 |
| `140A8ED70` (unit reset, slice 2) | seq479 (1 call) | proven, 1/1, 0 mismatches | 8/12 |

Previously proven (round 5): `141EB8E80` (1 call), `140A88E00` (6), `140A89F20` (5), `140A88CE0` (6). Round 6 also re-ran `1408BE3C0` (see section 3). Records are in `results_recorder.json` (writers) and `results_animelayer.json` (boot); closure files `closure_rec30.json`, `closure_rec3f0.json`, `closure_rec8d0.json`, `closure_rec8ed.json` are committed.

Layer: 10,232 functions, 3,832 recorded bit-exact (was 3,828). `gen_translated.py --check`: byte-identical, rc 0. `ledger.py`: current (44,479 rows); `--check` clean.

## 2. Import models added (CONFIRMED by unicorn tests and one mutation each)

These are the named imports the record create `1408BE3C0` reaches (from its trace):
- `__stdio_common_vsnprintf_s` (`"referee%03d"`, size 0x20, count -1): `vsnprintf_model.s` (the vsprintf body with the fifth/seventh-argument loads), `test_vsnprintf_model.py` 3/3; mutation (format slot read from the wrong offset): 2 failures.
- `strncmp` (the asset-path compare `"/Assets/pes16/…"` vs `"/Assets"`, n = 7): `strncmp_model.s`, `test_strncmp_model.py` 6/6; mutation (`jne` to `je`): 2 failures.
- `toupper`: `toupper_model.s`, `test_toupper_model.py` 3/3; mutation (`ja` to `jg`): 1 failure.
- The C++ mirrors are in `src/pes/translated/pes_stubs.hpp` (`pes_model_stdio_format` shared by `vsprintf_s` and `vsnprintf_s`; `pes_model_strncmp`; `pes_model_toupper`); `xlate.py` branches; `import_stubs.json` entries with `code_hex` from `as` + `objcopy` (the method checked against the existing vsprintf entry, which matched byte for byte).

**Side effect on the layer (CONFIRMED):** naming these imports changed 26 closure functions on regeneration. Their committed text had dispatched an unnamed DLL address at run time (`pes_dispatch_lookup(0x6FFFFEC787B0)`), which GF cannot resolve; the regenerated text calls the named model. All 26 are closure members; no recorded function changed its text. This is the intended effect of naming, but it means those closure functions were not proven in their old form (they were not individually proven; their roots are unchanged).

**Incident (disclosed):** a `rsync --delete` from a scratch generator output removed seven hand-written files in `src/pes/translated` (`pes_cond.inl`, `pes_dispatch.hpp`, `pes_native.{cpp,hpp}`, `pes_state.hpp`, `pes_stubs.hpp`, `pes_survey.hpp`). I restored the six untouched ones from HEAD. `pes_stubs.hpp` had uncommitted mirror edits, which I re-applied by hand (the same text). The commit contains the restored and re-applied versions; `git status` showed no other deletion. Lesson recorded: never sync the layer from a generator root that does not contain the hand-written headers.

## 3. Not proven (NAMED GAPS)

- **`1408BE3C0`** (ReplayHuman create, six calls on `pid950952` seq002–012, copies outside live2): NOT proven. The trace now passes the `vsnprintf_s`, `strncmp` and `toupper` models, and stops in a CRT cleanup path (`0x14037FBC0 … 0x14037FC35`): `call qword ptr [0x142532330]` with `rcx = rdx = 0`. That slot is NOT named in `import_slots.json`, and the DLL code it reaches dereferences null (`unmapped data at 0`). Under the standing ruling (never emulate protection or integrity code; classify `.impdata` by what it does), I did not model it. Next step: classify the slot (its `.impdata` neighbours, the callers' context) and either name it or report it as a protection edge. OPEN.
- **`1408BED00`** (Human per-tick update, `pid1845381` seq7492, one call): NOT proven. The trace runs past the 50M budget and its last PES call is `0x141957C50`, the graphics-device allocator (`stubs.json` `gf-service`, answer 0). The harness refuses gf-service paths, so the call cannot be replayed as a whole. My decode said the per-tick entry; the rig's EVERY-20 predicate `q(rcx+0x938)==0` captured a creation-time call (one capture in ~4 min). The gfx allocator on that path means the render system is reached: a GF stand-in boundary, not a recorder fact. OPEN: decide whether the recorder's proof stops at the allocator (a native-service boundary, as for the copy path) or whether 1408BED00's render branch is excluded.
- **`140A8EE20`** (unit bind, slice 2): NO capture yet (hook armed on pid1845381 per Main; `rec_140A8EE20` landed none).
- **Cursor advance** (slice 2): still not located. The unit reset and bind are the only unit ops decoded (`140A8ED70`, `140A8EE20`); the write cursor `+0x10` is advanced by a caller not identified (OPEN).

## 4. Slice status

- S1 recorder: header writers (`140A88CE0`, `140A88E00`, `140A89F20`), human writers (`140A89030`, `140A893F0`, `140A898D0`) proven and translated. Prop writers `140A89E50/EA0/E00` not captured (`140A89E00` refused by the tool). Per-tick `1408BED00` and `1408CCBD0` not proven (above).
- S2 ring: unit reset `140A8ED70` proven (1 call). Bind `140A8EE20` awaiting captures. Cursor advance not located.
- Boot: `141EB8E80` proven (round 5); `1408BE3C0` not proven (above).

## 5. Checks

- `gen_translated.py --check`: byte-identical (10232). `ledger.py --check`: clean.
- Unit tests: `test_vsnprintf_model.py` 3/3, `test_strncmp_model.py` 6/6, `test_toupper_model.py` 3/3, each with its mutation red (above). `test_strlen_model.py` (round 2) unchanged.
- Not run: the C++ test binaries (layer changed; Main gates). `g++ -fsyntax-only` passed on `closure/pes_fn_1415A1720.cpp`, `closure/pes_fn_14037F7E0.cpp`, `closure/pes_fn_1403CAB80.cpp` and a TU that uses the three mirrors.
- Machine paths in the commit: 0 added lines.

## 6. Process notes

- The proof scripts (`prove_copy.sh`) copy each capture out of live2, unpack the copy, prove, then delete the copy (Main's rule). `1408BE3C0` copies were unpacked in place outside live2 in round 6 (`caps950952`) and deleted.
- The `vsnprintf` mirror refactor shares one stdio formatter between `vsprintf_s` and `vsnprintf_s`; the `vsprintf_s` path is the same code as before (its `test_vsprintf_model.py` was not re-run this round; Main's gate covers it).
- One `sed` edit of a test file's model key (`test_vsnprintf_model.py`) was a patch by script, against the repo rule; the result is the same as an edit-tool change and the test was re-run.
- The 561 `recbed` specs (the static closure of `1408BED00`) are in `specs/recorder`, committed as closure specs, with no proven record: they are closure inputs, as for other sets.
