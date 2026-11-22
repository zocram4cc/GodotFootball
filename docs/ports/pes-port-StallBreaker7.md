# pes-port-StallBreaker7: the action-slot targets, the layer gate, and the sim verdicts

Branch `StallBreaker`, pushed to `d31435fd`. Earlier commits: `f3b6d44e` (layer regenerated from the merged inputs), `ffc8b712` (the three handlers as edges). Patch of an unlanded fix: `<workdrive>/gf-StallBreaker-scratch/r7/pad-vtable.patch`.

## 1. Every target of the action object's +0x90 slot (CONFIRMED, static)
- The dispatch at `pes_fn_1406F6CE0:1078` reads `[obj]` (the vtable) then slot `+0x90`.
- The three known handlers sit in the vtables of `anime::match::action::Dodge`, `Sliding` and `Tackle` (type names from the image's MSVC RTTI: TypeDescriptor `.?AV...@action@anime@match@@`, COL at `vtable[-1]`). `0x140770290` is a `goal_keeper::SavingBase` family slot.
- Enumeration over the image (`/tmp/stalldrift/actvt.py`, output `actvt.json`): 96 action type descriptors, 94 COLs, 94 vtables. One candidate (`0x6942747069726353`, string bytes) is not code and is dropped.
- Distinct code targets of slot `+0x90`: **35**, all in `INDIRECT_EDGES[0x1406F6CE0]`. `0x141794410` is `DemoAction`'s slot, shared by 42 vtables.
- Not proven: these are closure members, not recorded bit-exact. The slot is read from the image, not executed; no per-target proof was built.

## 2. The layer fix that made the gate pass
- Root cause of the drift (from the prior commit): the merge's layer was hand-resolved after the green check (six stale `closure/` duplicates; four files kept HEAD's text where the merged inputs produce StallLive's).
- `d31435fd`: 25 new closure files, 6 changed, `ro_tables.json` +4 entries (the lookup tables that `0x140770290` walks; extents from capstone on the image's bytes: `0x1425E7850` 0x20, `0x1425E76B0` 0x38, `0x1425E76E8` 0xC, `0x1425E76F8` 0xC).
- Walk change: a root's own indirect targets join its universe (`universe |= INDIRECT_EDGES[va]`). Without it, two tail jumps (`0x1407A2660`, `0x1407A3190` -> `0x1407A1490`) leave the closure and `gen --check` refuses.
- `gen_translated.py --check`: **rc 0, 10,499 functions (3,865 recorded bit-exact, 6,634 closure) byte-identical**. No untracked files (`git ls-files --others` empty after the commit).

## 3. Sim verdicts (build from `d31435fd`, seed-7 / seed-11 matches, `env -u GF_DEBUG_*`)
| test | result |
|---|---|
| PesRestartDelay.EveryStoppageRestartsWithinTheInstallBound | PASS (78-106 s) |
| PesRestartDelay.EveryStoppageRestartsWithinTheInstallBoundWithPesLoop | FAIL, no abort: `s.gameOver` false after 2,134 s (match did not reach full time) |
| PesRestartDelay.PesLoopRestartsAreArmedAtTheirPesSpotsSidesAndReleaseFrames | ABORT, `PES_DISPATCH_UNTRANSLATED: 0` |
| PesStall.ALiveBallNeverLiesStillLongEnoughForTheBreaker | FAIL, no verdict: `census.stop.gameOver` false after 2,159 s; the breaker census is not reached |
| PesHang.SeedElevenTwoMinutePesLoopMatchReachesFullTime | ABORT, `PES_DISPATCH_UNTRANSLATED: 0` |

**ALiveBall has no verdict.** The match does not reach full time in ~36 min, so the assertion stops the test first.

## 4. The null key 0 (CONFIRMED site, cause NOT established)
gdb (`pes_hang_test`, `d31435fd`):
```
pes_dispatch_missing(t=0)
 <- pes_fn_141B52380 closure/pes_fn_141B52380.cpp:22
 <- pes_fn_1406EC660 closure/pes_fn_1406EC660.cpp:83
 <- pes_fn_1406E2C30 closure/pes_fn_1406E2C30.cpp:72
```
- `141B52380` reads the global `0x1437F0A58` (the pad manager, `kPadManagerGlobalVa`, `pestrapdata.hpp:34`). If non-null, it reads `[obj]` (vtable) and dispatches the word at `vtable+0x68`. Key 0 means that word reads 0 in GF memory.
- Hypothesis tested: the loader's pad object points at the image vtable `kPadVtableVa`, and GF maps no image, so slot `+0x68` reads 0. Fix (`pad-vtable.patch`): an arena vtable copy holding `*padMaskFn`, plus the oracle test's compare moved to slot `+0x68`. Built; **PesHang still aborts with key 0 at the same movestep** (`A2 (17)`). The hypothesis is **refuted or incomplete**. The fix was reverted per the land-or-revert rule; the patch is kept.
- Open: which object is at `0x1437F0A58` in the sim when the abort fires (is it the loader's pad object, or another writer?). Next step: a gdb `print` of `*(uint64*)(s.mem + 0x1437F0A58)` and of the object's first qword at the abort, and a grep for every writer of `0x1437F0A58` in the GF runtime (only `pestrapdata.cpp:349` is found in `src/`).

## 5. Gaps and open items
- Handlers: unproven closure members (no synthetic-state proof). The 35-target set: unproven, slot read from the image.
- Abort at key 0 blocks PesHang, PesRestartDelay test 3, and ALiveBall's census. The `gameOver` failures in tests 2 and ALiveBall may be the same stall, but the abort is the only hard evidence.
- No red-first test for the pad fix (not landed, so none required).
- Process: `build/` was removed at the end of the previous round and the test binaries had to be rebuilt from scratch (~75 min). Keep it, or accept the cost.

## 6. Commits
- `f3b6d44e` layer regeneration (gen --check byte-identical).
- `ffc8b712` three handlers as INDIRECT_EDGES.
- `d31435fd` 35 action-slot targets, the root universe rule, `ro_tables` for `0x140770290`'s four tables.
