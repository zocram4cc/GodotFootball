# PES port MergeLane2: step-2 merges (PressCut, MergeBKBJ, PassArrival) on the StallLive base

Branch `MergeLane2`, pushed `db9bfef3` (`git ls-remote origin MergeLane2` confirms). Base: StallLive `1b56b726`.
Merge commits: PressCut `44688747`, MergeBKBJ `4425ef0f`, PassArrival `c25b4938` (no conflicts). Fix commit `db9bfef3` (pestrapactor.cpp).
Status legend: CONFIRMED = tool run this session; INF = inference; OPEN = blocker named.

## 1. Gates after each merge (CONFIRMED)
| check | after PressCut | after MergeBKBJ | after PassArrival |
|---|---|---|---|
| `gen_translated.py --image pes_full.bin` (regen) | 10,347 functions | 10,637 functions | no translated input changed |
| `gen_translated.py --check` | byte-identical (10,347) | byte-identical (10,637) | byte-identical (10,637), rc 0 |
| `tools/dev/ledger.py` then `--check` | rc 0; 0 regressions vs HEAD (163 recorded scope moves) | rc 0; 0 regressions (163 moves); LEDGER current 47,642 rows | rc 0, LEDGER current |

The ledger regression seen at the first PressCut regen (9 rows `engine` -> `undecoded`) was my own error: I had taken PressCut's `universe.json` whole. Restored with a hunk-level resolution; the rows came back and the check went green.

## 2. Resolution decisions (by intent; the reports name each branch's design)
PressCut (`docs/ports/pes-port-PressCut.md`): the press dispatcher runs on the window (`PesWorld::RunPressDispatch`); the GF hand port is deleted.
- `teamAIcontroller.cpp` (source conflict, PesPressTick): took PressCut's block. HEAD's loose-writer holder rule is the rule the window reads (`f.holder = GetPesHolder`). Diff vs `origin/PressCut` leaves HEAD's non-press OneWorld edits only.
- `ro_tables.json`: union by VA (HEAD's 2 rows + PressCut's 12; 86 rows, none lost). The HEAD rows gained the separating comma.
- `universe.json`, `closure_*`, `translated/*`, `LEDGER.md`: regenerated or derived (see 4).

MergeBKBJ (TrapContact/BJ, PassReceiver):
- Anime chain is the engine's: HEAD's OneWorld one-chain cut (`BindAnimeChain`, `WriteAnimeG`, `PesAnimeIssue`, the engine's `P+0xA08`). MergeBKBJ's window-owned chain (`PhysiqueA/B`, the layer-rate block of TrapContact9 in `ConstructActorP`) was dropped. HEAD's engine owns the chain. A ponytail-free removal: `PhysiqueA` was not declared anywhere in the merged tree, so the block would not compile.
- Trap clip issue: HEAD's request-based `PlayIssuedClip(MotionRequest)` (it pushes the record into the engine) kept; MergeBKBJ's `NotePesTrapTickIssue` and the dribble re-issue (`PlayDribbleIssue`) added.
- Additive resolutions (both kept): `pesworldrecord.hpp/.cpp` (kick and PassGet window code, `RunTrapAdmit`, `EventBytes`/`ResetEvents`, `ImageRun`/decision blocks), `matchevents.cpp`, `elizacontroller.hpp/.cpp` (`_PressApproach` + `_PassGetApproachGf`), `humanoid.cpp` (throw-in and dribble guards combined in one condition), `humanoidbase.cpp`, `match.cpp` (ankle shove, then dribble ticks, actor counters), `pes_stall_test.cpp` (both probe sets; HEAD's `GF_STALL_DUMP` probe kept).
- `PesWorldImage`: `press`, `squad`, then `passGet`; `kPassGetOffset` now follows `kSquadOffset` (the old formula assumed `passGet` directly after `ai`).
- `heap_`: HEAD's `unique_ptr<PesHeap, PesHeapDeleter>` (non-owning when borrowed) kept; MergeBKBJ's duplicate declaration dropped, its six `make_unique` sites ported to HEAD's `heap_.reset(new PesHeap())`.
- Not in a conflict but broken on the merged tree: `pes_native.hpp` `Host()` used `arena_.size()`; ported to HEAD's raw arena pointer and `arenaBytes_`.

## 3. Test table (CONFIRMED; `env -u GF_DEBUG_RECEIVE -u GF_DEBUG_ANKLE -u GF_PES_SURVEY`, PES21_ROOT=<workdrive>/PES21, PES_EXE=pes_full.bin, via simslot)
| binary | result | which merged branch the test is from (INF from the file's history) |
|---|---|---|
| pes_press_world_test | PASS 29/29 | PressCut |
| pes_press_kickoff_test | PASS 6/6 (was abort 0x140789D00 before the fix) | PressCut |
| pes_press_test | PASS 5/5 | PressCut |
| pes_playerdb_wiring_test | PASS 14/14 | PassArrival (Acceleration byte) |
| pes_press_approach_test | ABORT rc 134, `PES_DISPATCH_UNTRANSLATED 0x14073DD50`, 1 test started | PressCut/MergeBKBJ (approach) |
| pes_pass_target_test | ABORT rc 134, `0x14073DD50`, aborts before the first test | PassArrival |
| pes_throwin_release_test | ABORT rc 134, `0x14073DD50`, 1 test started | MergeBKBJ (throw-in release) |
| pes_movestep_wire_test | ABORT rc 134, `PES_DISPATCH_UNTRANSLATED 0x140791DD0`, 3 started, 2 OK | PassArrival / MergeBKBJ |
| pes_restart_delay_test (contains pes_stall_test.cpp) | ABORT rc 134, `0x14073DD50`, 2 started, 1 OK | StallLive / MergeBKBJ |

Not run: `pes_stall_test` as its own run beyond the restart-delay binary (it aborts there), gfsim smokes, the full gate (Main gates).

## 4. Fix made on the merged tree (CONFIRMED)
- `db9bfef3`: MergeBKBJ's `AnimeActionOfClip` (auto-merged) wrote kick-family action ids (9, 8, 0xA, 0xB, 0xD, 0x38, 0x3A paths) into `P+0xAC8`; their PES action ticks are untranslated, which is the abort HEAD's own revert `694e354e` documented (`0x140789D00`). Kick-family function types now return the MOVE stand-in with a `ponytail:` comment. Effect: `pes_press_kickoff_test` 0 -> 6/6. Trap, dribble, movement and special families unchanged.
- Data files, not hand-merged: `universe.json` = HEAD's rows (the 47,642 VAs; MergeBKBJ had no VA that HEAD lacks, checked: 0 missing) plus MergeBKBJ's closure meta entries, written with the tool's own serializer format (`indent=1, sort_keys`). The Ghidra bridge (port 8080) is down (HTTP 000), so `universe.py` could not regenerate it; this follows Main's ruling (union by VA, no hand edit). `results_trapaction.json`: union of proof records (78; HEAD 35 + MergeBKBJ-only). `closure_trapfinish_f560.json`: HEAD's 86 members (MergeBKBJ's 6 are a subset). Those two are proof records, not regenerable here (captures and bridge), so the union is the honest state; OPEN: re-prove.

## 5. Reds, attribution
- Abort `0x14073DD50` (4 binaries) and `0x140791DD0` (movestep_wire): CONFIRMED untranslated PES dispatch; `0x14073DD50` is a leaf accessor (ledger: `leaf accessor`, callers `1406DF630` / `1406E0540`, the latter a PES action-object builder). No translated closure in StallLive, PressCut or MergeBKBJ contains it (`git grep` of each tree's `src/pes/translated`: no hit). So the route is not one a single branch translated: INF it is reached through a body action object once a body is given an id whose object is built from `1406E0540`. Owner: OPEN. Not attributed to a branch by a per-branch build (not run; budget).
- Pre-existing reds per branch: NOT MEASURED. Every number above is the merged tree. The attribution experiment: build `origin/MergeBKBJ` (`abe9f81d`) and `origin/PassArrival` in their own worktrees and run `pes_pass_target_test`, `pes_press_approach_test`, `pes_throwin_release_test`, `pes_restart_delay_test`; a pass there means the abort is merge-induced by PressCut's or StallLive's code.

## 6. Killed theories
- The `0x140789D00` abort was not a PressCut dispatcher bug: removing the kick-family body ids removed it everywhere (kickoff green; no run reaches it now).
- The 9-row ledger regression was not a classification change: it was my `universe.json` resolution (fixed by the hunk-level restore).
- The `PesWorldImage` layout assert failure was the `passGet` offset formula; the static assert now follows `kSquadOffset` and compiles.

## 7. Open items (numbered)
1. `0x14073DD50` untranslated leaf reached by the action-object path: translate `0x14073DD50` (and check `1406E0540`/`1406DF630` closures) or find the body-id writer that reaches it. Owner: whoever owns MergeBKBJ's action path (TrapContact).
2. Per-branch attribution of the four `0x14073DD50` aborts (experiment in section 5).
3. `pes_movestep_wire_test` `0x140791DD0`: a second untranslated action tick; not traced.
4. `results_trapaction.json` and `closure_trapfinish_f560.json`: union / HEAD side; re-prove from captures when the bridge is up.
5. `universe.json` MergeBKBJ closure meta: rows are complete; re-run `closure.py --tag` to restore the MergeBKBJ meta record set if needed.
6. Later StallLive tips: merge on request from Main (lane is at `db9bfef3`).
