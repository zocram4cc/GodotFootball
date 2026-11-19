# BootAtexit: atexit is a return-zero stand-in; its onexit dispatch leaves the translated layer

Status: DONE on branch `BootAtexit`, commit `1b8746c0` (pushed; `git ls-remote origin BootAtexit` = `1b8746c0`), on origin/main `be6e1ffa`. Implements docs/re/pes-audit-BootDispatch.md section 6.

## What changed (clean cutover)
- `tools/dev/translate/stubs.json`: new entry for 0x14159F6C0 (atexit), kind `return-zero`, op `atexit`. Its `why` carries the classification: both `_onexit` branches are protector-owned (slots 0x142530348 / 0x142530318), the selector is cell 0x143706720 (live: enc == ~cookie, branch 0x1415A18FA), the owner's .impdata ruling (17-11-26), and the exit-destructor loss (section 5, OPEN).
- `tools/dev/translate/stubs.py`: `return-zero` -> `return0` in DO_BY_KIND, KNOWN_DO and MODEL_DOS (every spec set models it the same way).
- `tools/dev/translate/xlate.py` (`stub_lines`): `return0` emits `s.gpr[0] = 0;` at each call site, with a `ponytail:` comment naming the selector, the slot and the loss.
- `tools/dev/diffharness/run.py`: the harness models `return0` (rax = 0; the return is popped as for `noop`).
- Closure seeds dropped: `closure_danger.json` (0x14159F670, 0x1415A18F4, 0x1415A18FA), `closure_trapdribble.json` (0x14159F6C0).
- Spec removed: `tools/dev/diffharness/specs/core/14159F6C0.json` (the universe refuses a VA that is both a model and a spec; the generator said so). Its two lines in `specgen/sets/closure-danger.txt` and `closure-trapdribble.txt` removed.
- Regenerated with `tools/dev/translate/gen_translated.py` (never by hand): 4 closure files deleted (pes_fn_14159F670, pes_fn_14159F6C0, pes_fn_1415A18F4, pes_fn_1415A18FA); 6 modified (closure callers pes_fn_14022D620, pes_fn_14038EE80, pes_fn_1403D2BB0, pes_fn_1403FB240; `pes_dispatch_gen.cpp`; `translated.hpp`). No dispatch or declaration entry for the four VAs remains (grep: 0).
- Not touched: `ZeroInitializedData`, `FUN_14159F4B8`, pesboot.cpp (it is on the OneWorld branches, not main).
- The generator's "held out" list (3 functions: 0x1403BB670, 0x1405549C0, 0x14202BA30) was already absent on main and is unchanged.

## Verification
- `gen_translated.py --check --image pes_full.bin`: **5648 functions (3758 recorded bit-exact, 1890 closure) regenerate byte-identical**, rc 0 (`diff_translated_drift` equivalent).
- `tools/dev/diffharness/check_closure_stubs.py --image pes_full.bin`: GREEN for 0x140A39A80 and 0x14044C880 (0x14044C880's closure now lists 14159F6C0 as a model).
- `tests/pes_fox2_test` (fox template / boot-adjacent): **4/4 PASSED**, rc 0.
- Smokes, seed 7, 2 minutes, run in sim slots on the same worktree, base binary vs new binary:
  - switch-off: base `[determinism] a0fbe2d5cd445a52` (24539 ticks) -> new `a0fbe2d5cd445a52` (24539 ticks). **Unchanged.**
  - pes-loop: base `17941efc16b4a6a5` (29532 ticks) -> new `17941efc16b4a6a5` (29532 ticks). **Unchanged.**
  - So GF's answer did not move: the hashes stay at the base values measured in this worktree.
- `tools/dev/translate/test_buildcache.py` (harness Python): 35/35 OK.
- `tools/dev/ledger.py --check`: green (no regression; 38338 rows). `docs/re/LEDGER.md` unchanged: the atexit row stays `engine`.
- Not run (the gate's job): the recorded diff suites (`diff_core_recorded`, `diff_pilot_recorded`, `diff_cutscene_recorded`, the diff_*_recorded family). The stub model is part of their cache keys, so they re-check cold; run in the merge lane.
- Not run: the full gate. The four changed `tests/diff` binaries were not rebuilt (none name these VAs).

## Notes for the merge
- The change takes effect at boot only when pesboot.cpp (OneWorld branches) runs `pes_fn_14159F6C0`'s callers; on main nothing calls the boot. The callers of atexit in the layer are the four closure members above.
- Registers: the stub clobbers only rax. The old path left rdx/r8..r11 as the callee chain set them; caller-saved registers are dead after a call, so the callers are unaffected.
- Behaviour kept: the old stand-in answered 0 (the chain returned `param != 0` - 1 = 0), and so does the new stub.

## Open (unchanged by this change)
- Destructor review (721 of 726 distinct exit-time destructor targets not read): callee review for file, save and exit APIs. Method in docs/re/pes-audit-BootDispatch.md, section 5.
- Dynamic count of atexit calls at GF boot (1,678 vs 29 static reach): instrument the stub. Method in the audit, section 4.
- The four closure callers' recorded proofs: their diff records (through the recorded roots whose closures reach them) are re-checked by the gate; not re-proven here.
