# PES port: AOFix, the diff build treated a switch case as a callee (29-10-26)

Branch `AOFix` (worktree `<workdrive>/gf-AOFix`) from `origin/MergeBatchAO` 1bf77938. Gate not run (Main gates). CONFIRMED = read in tool output.

## Root cause (CONFIRMED)
`driver.build_and_diff` added **every in-code target of the harness trace** (`trace_<VA>.json`, indirect site -> entered address) to the build closure and translated each as a function. The trace records `jmp reg` switch targets too: for 0x140A61F40 it is `{"0x140A61FF8": {"0x140A62047": 79, "0x140A6204C": 2}}` (the switch at 0x140A61FF8; 0x140A62047 is its `case 12` label inside 0x140A61F40). Before the shared extent rule (`pdata.py`, StoppageFlow), `capstone_decode` decoded from any address, so the label became a stray "function" and nothing failed. With the extent rule `capstone_decode` returns nothing for a non-boundary, so the build failed `callee 0x140A62047: no instructions` -> `translator bug`. The same fault class applied to every function whose harness trace takes a `jmp reg` case. It was not the layer: the generated 0x140A61F40 holds only `goto L_140A62047`.

`snapdiff.build_closure` already had the correct rule (skip a `jmp reg` target in the same body as its site; a non-boundary target elsewhere is an error); the driver never got it.

## Fix
`driver.reached_roots(image, reached)` (one rule, `(roots, error)`): in-code targets that start a function; a `jmp reg` target in the same body as its site is no root; any other non-boundary in-code target is an error naming it; targets outside `.trace` are left to the loud dispatch abort. `driver.build_and_diff` and `snapdiff.build_closure` both call it (snapdiff's private copy and its own `boundary_map` are deleted; it uses `xlate.pdata_of`). `driver.CODE_LO/HI` named once.

## Tests (`test_closure_walk.py`, 18 tests; 5 new)
Red: the new `driver.reached_roots` tests do not exist on 1bf77938 (AttributeError); the real failure is Main's `pilot-check/results.json` (both VAs `translator bug`). New: switch case in its own function is no root; function start kept and instruction-middle refused; register-jump case in a pdata-less leaf is no root; target outside the code range is no root; with `PES_EXE`: the real trace `{0x140A61FF8: {0x140A62047, 0x140A6204C}}` yields no roots and no error.
Mutations (scratch copy, each run and discarded): M1 no same-body skip, M2 non-boundary becomes a root, M3 boundary error off, M4 no code-range filter (survived at first; test added, then killed), M5 snapdiff ignores the shared error. **5/5 killed.**

## Verification
- `driver.py --only 0x140A61F40 0x140A62140`: **both bit-exact** (81 and 55 cases).
- Full pilot recorded check (`driver.py --check --record results.json`, the `diff_pilot_recorded` command): **245/245 still diff bit-exact**.
- `gen_translated.py --check`: **4075 functions (3687 recorded, 388 closure) regenerate byte-identical**; the layer is unchanged, nothing regenerated. No recorded function's closure contains 0x140A62047 (it is a label, not a function).
- `test_extent.py` 19/19, `test_closure_walk.py` 18/18.
- Not re-run here: cutscene, presentation, core, impure, fixdemo recorded sets (Main's AO gate runs them on 1bf77938 without this fix). Any of their functions whose trace takes a same-body `jmp reg` case would have failed the same way (`no instructions`); the pilot set is the only one Main reported red. Re-run them on AOFix if any shows that note.

## NativeSeam merge: not done
`git merge-tree` of `origin/NativeSeam` into AOFix conflicts (layer files regenerated on both sides, `translated.hpp`, `pes_dispatch_gen.hpp`, `driver.py`). Left unmerged as instructed; NativeSeam needs a merge with the layer regenerated afterwards (its `driver.py` also defines `CODE_LO/HI` and a `runnable_extra(universe=)`, so the conflict is mechanical but the regenerated layer needs `gen_translated.py` on the merged tree, and `reached_roots` already provides the rule its closure walk would want).

## Fast-tier line (interim; `GATE_TIER` not on this branch)
`GATE fast (interim): GREEN` for: `diff_pilot_recorded`-equivalent 245/245, the two VAs, `gen_translated --check` byte-identical, `test_closure_walk.py` 18/18, `test_extent.py` 19/19. Not run: the `pes_*` C++ binaries (no C++ changed), the other recorded sets, smoke (Main).

Branch pushed: `AOFix` (see `ls-remote`).
