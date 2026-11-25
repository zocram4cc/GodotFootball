# RecordLane3: driver --record keeps every row a run did not attempt (M6); CD90 spec gap diagnosed, not fixed

Branch `RecordLane3` (from origin/Integrate a316278d1), worktree <workdrive>/gf-RecordLane3.
Pushed tip: `1735ddaae` (confirmed `git ls-remote origin RecordLane3`). Earlier: `07f50a508`.
Scenario claimed: **M6** (harness/tooling fix: root cause named, records re-checked, statuses only fail -> pass).

## 1. Root cause (CONFIRMED)

`driver.py` wrote `--record` only on a full run, and then wrote the attempted rows only. So:
- `--exclude-recorded` dropped every row of the record that another results file also records (the 23-11 trap 1);
- `--only`/`--set` runs never wrote the record (trap 2).

## 2. Fix (driver.py, test_record_merge.py)

- `merged_record(existing, results)`: every existing row stays in its position; an attempted VA's row is replaced in
  place; VAs the file lacks are appended in run order. `--check` still never writes.
- The first version sorted the whole file. That rewrote every row's position (3183 rows, ~5k diff lines for 3
  replaced rows); HEAD's order is the `json_union_merge` order, not sorted. `1735ddaae` fixed it, with a red test.
- Docstring and `--record` help updated.

## 3. Tests (red first, then green)

`tools/dev/translate/test_record_merge.py`: drives the real `driver.main()` with only the per-function run stubbed,
on a temp spec dir and fake record. 3 tests, `python3 tools/dev/translate/test_record_merge.py`:
- `--only` replaces its row, keeps the others, and keeps the record's order. Red on the old write (record lost the
  attempted row's verdict) and red on the sort (order).
- `--exclude-recorded` (with `recorded_elsewhere` naming 0x2000) keeps 0x2000. Red on the old write (rows dropped).
- `--check` never writes the record. Green on old and new (guard).
- Result now: `Ran 3 tests ... OK`, no ResourceWarnings.

Mutations (each run, reverted, re-run green):
- M-a: `return sorted(results, ...)` (drop kept rows): 2 FAIL (`--only`, `--exclude-recorded`).
- M-b: `sorted(kept + results)` (sort the whole file): FAIL on the order assertion, `['0x1000','0x2000','0x3000'] != ['0x3000','0x1000','0x2000']`.

## 4. No-op check on committed records (CONFIRMED)

Run: `driver.py --image pes_full.bin --spec-dir specs/core --set specgen/sets/core.txt --only 0x14063CD90 0x1400020B0 0x1400100D0 --record results_core.json`
(GF_DIFF_PYTHON = the DiffHarnessRE venv). Result: 3 attempted, 2 diffed (0x1400020B0 bit-exact 64/64, 0x1400100D0
bit-exact 64/64, own-block coverage 1.00); 0x14063CD90 `harness cannot run` (unchanged).
`cmp` of the record against `git show HEAD:...results_core.json`: **byte-identical**.
Two earlier attempts that I rejected:
- without `--set`: the attempted row lost `subsystem`/`role` (driver writes attribution only with `--set`);
- with the first sort-based merge: 5k diff lines from the re-sort (fixed by `1735ddaae`).

Not re-checked: the other 40+ results files. The write path is the only change; no spec, translation or status
changed in any record.

## 5. CD90 (0x14063CD90): spec gap diagnosed, NOT fixed, no spec regenerated, no status moved

Status now: `results_core.json` row `diffed: false`, `failure_class: harness cannot run`, note
`1 valid of 64 requested; 121 failed skipped` (unchanged).

Failure (CONFIRMED, `build-core/gen/14063CD90/corp_14063CD90.jsonl.meta.json`): `unmodelled read at 0x1b (size 8):
no region or global covers it`. In the failing case `stk0 = 27`.

The brief's premise is not supported by the code. Evidence:
- **rdx is not a record pointer (CONFIRMED).** At entry `rdx` is spilled to [E+0x10] (14063CD9B) and reloaded at
  14063D069 (`mov rcx,[rbp+0xa8]`) and passed as rcx to `0x140A53DF0`. That callee overwrites rcx with r8 before any
  use (140A53E46), so the value is dead. At 14063CE8E `rcx = r14 (= rdx)` goes to `0x140A62690`, which overwrites rcx
  with `[IMG+0x270]` at entry (140A6269A). Nothing in the closure dereferences the rdx value. The rcx reloaded at 14063D069
  is overwritten before the output store (14063D126 uses `ecx = count`). So `scalar_rdx` is a dead input, not a
  missing region.
- **The record pointer is stack argument 0, the 5th argument (CONFIRMED).** `mov rbx,[rbp+0xc0]` (14063CDC1) loads it,
  and CD90 dereferences `rbx` at +0x0/+0x8 (14063CE4D/CE55), +0xC (CE51), +0x10 (D0A8/D0F8), +0x14 (CF9B/D084),
  +0x18 (CE7A). The walker records these as `unresolved-mem` (its incoming stack args have no origin), so the
  spec has no field for them. It emits `stk0` as a scalar u64 with the generic 0..255 domain. The harness then
  writes 27, and the read at 0x1b fails.
- **r9d is signed (CONFIRMED use):** `movsxd rdi, r9d` (14063CE36). r8d (INF): reloaded from its home [rbp+0xb0]
  (14063CF44, 14063D14C); signedness depends on the jcc after `cmp edi, r12d` (14063CEEE), not checked.
- **The brief's generic rule would be wrong.** `arg_roots` = {A1,A2,A3} because a walked callee receives those
  registers as ignored args (pass-through), so "any arg root -> region" would turn r8/r9 (ints) into pointers.

Why this is not a spec-generator-only fix: a pointer-valued *stack* slot needs a harness location kind.
`run.py set_inputs` (line ~400, `stack_offset`) and `_input_addrs` write stack inputs as scalars, and
`translate/diffmain.py` (lines 148-150) writes them the same way in the diff main. The fix needs run.py, diffmain.py, the
walker (incoming STK-arg origin) and spec.py together. I did not start it (70% budget rule; outside the spec
generator's file set).

Specs changed by a generator change: **none** (no generator change was made, so no record needed a re-check).

## 6. Verification run

- test_record_merge.py: 3/3 OK (see section 3).
- No-op check: byte-identical (section 4).
- `gen_translated.py --image ... --check`: see the final message (run in the foreground on the pushed tip, `1735ddaae`).
- PII: `git diff origin/Integrate -- tools` added lines grep for `<workdrive>` and `<home>` = no matches
  (`tools/check_pii.sh` not run: it fails inside a worktree because of the symlink).
- `gen_translated.py --image pes_full.bin --check` on tip `1735ddaae`: rc=0, "10999 functions (4160 recorded bit-exact, 6839 closure) regenerate byte-identical". It printed one hold-out: `0x140928FC0` (its closure reads read-only table live; declare it in ro_tables.json). Not compared with Integrate's hold-out list [INFERENCE: not new]; the gate does not fail on hold-outs.
- Worktree note: `extern/godot-cpp` is a gitlink in Integrate, so plain `git status` fails with the symlink in place. Path-limited git works (`git status -- tools`).

## 7. Unfinished (numbered)

1. CD90 spec: add a stack-slot pointer location (run.py `set_inputs` + `_input_addrs`, diffmain.py stack write), make
   the walker give incoming STK-arg bases a root, and emit `stk0` as that root in spec.py. Then regenerate
   `specs/core/14063CD90.json` (`closure.py --regenerate` for that VA), re-diff, and re-check every record whose spec
   changes. This is a harness change; not started.
2. r8d/r9d domains: `i32` for scalar register inputs used sign-extended or with signed jcc (r9d: movsxd confirmed; r8d:
   check the jcc). Needs diffmain support for `i32` reg inputs; not started.
3. Attribution: a `--record` run without `--set` resets `subsystem`/`role` of the rows it attempts (seen on 0x14063CD90).
   Left as is; the fix is to carry attribution over when `--set` is absent.
4. Full re-check of all results files after the write-path change: not run (only the core no-op above).
