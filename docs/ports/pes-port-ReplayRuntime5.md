# pes-port-ReplayRuntime5: the boot builder and three recorder writers are proven bit-exact on real captures and translated (gen --check green); 1408BE3C0 NOT proven (captures are packed under Main's auto-slim); the per-tick entries NOT proven

Branch `Presentation`, pushed `ac9ec418..8ced8e35` (`git ls-remote origin Presentation` = `8ced8e355ca7`, confirmed). One commit, `8ced8e35`: the layer (`src/pes/translated`, +4 recorded functions and their closure), `results_animelayer.json` and the new `results_recorder.json` (merged proof records), closure files (`closure_bootpath.json`, `closure_recce0.json`, `closure_rece00.json`, `closure_rec9f20.json`), the root specs (`specs/kick/141EB8E80.json`, `specs/recorder/`), specgen sets, universe rows, LEDGER. No machine paths in the diff (grep 0). No build dir was created; none to delete. Scratch 2.7 MB.

Labels: CONFIRMED = a `prove_bounded` replay over real captures (the record), `gen_translated --check`, `ledger --check`. INF/OPEN as marked.

## 1. Proofs (CONFIRMED; all with `--merge`, real captures only, no synthetic states)

| root | captures | result |
|---|---|---|
| `141EB8E80` (boot builder) | `pid950952/seq001` (1 call) | proven, 1/1 replayed, 0 mismatches, own-block 85/94 (90.4%) |
| `140A88E00` (recorder header) | `pid1337341` seq53848, 53898, 53949, 53998, 55858, 60069 (6 calls) | proven, 6/6, 0 mismatches, own-block 3/3 |
| `140A89F20` (recorder header) | `pid1337341` seq53849, 53899, 53950, 60070, 60119 (5 calls) | proven, 5/5, 0 mismatches, own-block 5/6 (83%) |
| `140A88CE0` (recorder header) | `pid1337341` seq53826, 53827, 53829, 53830, 53831, 53832 (6 calls) | proven, 6/6, 0 mismatches, own-block 13/13 |

The recorder closures are small (3, 1, 2 members from `closure.py --spec-set recorder`); each proof's closure is the set of functions its replays entered (`closure_<tag>.json`), which `gen_translated.py` reads.

## 2. Layer (CONFIRMED)

- `gen_translated.py` wrote 10,225 functions (3,828 recorded bit-exact, was 3,824; 6,397 closure). `gen_translated.py --check`: byte-identical, rc 0.
- Repo fix needed for the layer to read the new records: `gen_translated.py` `SETS` had no `results_recorder.json` (added). And `results_animelayer.json` maps to the `kick` spec set in `SETS`, so the boot root's spec lives in `specs/kick/141EB8E80.json` (moved from `specs/animelayer`; the `animelayer` spec dir is not read by the layer).
- `ledger.py`: LEDGER.md current (44,475 rows); `--check` clean.
- Tests NOT rebuilt this round: the layer changed (new functions in `pestranslated`), so any test that links the layer should be re-run by Main at the gate. Not run by me.

## 3. Not proven (NAMED GAPS)

- **1408BE3C0** (ReplayHuman create, six calls `pid950952` seq002–012): NOT proven. Cause: Main's auto-slim packed the captures while the proof ran. The first attempt failed (seq002 missing at trace time); a second attempt with all six unpacked also failed on seq002 (packed again mid-run). Action for Main: keep pid950952 seq002–012 unpacked (or give me a copy outside live2) and re-run `prove_bounded.py 0x1408BE3C0 … --tag bootcreate --merge --budget 50000000` with `GF_SNAP_BUDGET=50000000`. Its root spec is `specs/animelayer/1408BE3C0.json` (committed; not in the layer's sets, so the spec dir needs adding to `SETS` when it is proven).
- **Per-tick entries** `140927450` (refused by the hook tool, call at +9), `1408BED00`, `1408CCBD0`: NOT proven. Per Main, `rec_1408BED00` has not reached its EVERY-1000 sample point (fewer than 1000 in-play calls since launch). My decode (`pes-re-replay-system.md` §2) says `1408BED00` is the Human's update slot, called every tick per player, which would make 1000 a rare sample. That conflicts with the rig result. Either the decode is wrong (the call may be only on record creation, `P+0x938 == 0`), or the rate is lower than assumed. Not resolved. Proposed: sample `1408BED00` at EVERY 20 with `--when 'b(rcx+0x938)==0'` (creation only) to settle it.
- **`1408CCBD0`** (PropMain slot 6): a new capture is due about every 4 min (Main's note); not yet in my set.
- **`140A8EE20` / `140A8ED70`** (ring cursor, slice 2): not decoded further; no captures.

## 4. Slice status against the plan

- S1 recorder: header writers `140A88CE0`, `140A88E00`, `140A89F20` proven and translated. The human writers `140A89030`, `140A893F0`, `140A898D0` and the prop writers `140A89E50/EA0/E00` (`140A89E00` is refused by the hook tool, a `je` at +0xD) are NOT yet captured. The per-tick entries are the gap in section 3.
- S2 ring: not started (no decode of the cursor advance).
- Boot path (round 4's brief): `141EB8E80` proven and translated; `1408BE3C0` not proven (section 3).

## 5. Process notes

- `git -c status.submodule=none status` prints the `extern/godot-cpp` error and nothing else in this worktree; use `diff --name-only` / `ls-files -o`. This hid untracked layer files for part of this round.
- Two wasted attempts: a `prove_writers.sh` chain ran before its results file existed (fixed by creating `results_recorder.json`), and the 1408BE3C0 unpack loop skipped `seq002` (no effect on the others).
- My earlier wait loops matched stale `check rc=` lines; fixed by waiting on file mtimes.
