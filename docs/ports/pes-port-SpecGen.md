# SpecGen: closures from the image in one command; the EndClear chain does not land yet (28-10-26)

Branch `SpecGen`, worktree `<workdrive>/gf-SpecGen`. Remote head `d14dc42b` (`git ls-remote origin SpecGen` confirmed). Commits:
- `34d11748`: merge of `origin/EndClear` (`83022c25`) into SpecGen. Brings the EndClear records (diffed false, "harness cannot run"), their specs in `specs/core` (142109EB0, 142108630, 14063A290, 14063A490), `results_endclear_*.json`, RecordMap files. The old SpecGen head `c418423a` was an ancestor of main; reset with `-B`, nothing lost.
- `d14dc42b`: `tools/dev/translate/closure.py` (new), `gen_translated.py` (refusal lists every conflict), `tools/dev/diffharness/README.md` (command and status).

Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled.

## 0. Verdict

- The tooling part is done and pushed: one command takes root VAs and makes their closure regenerable from the image (no bridge): universe rows, pilot specs, set file. CONFIRMED by a 86-member run.
- The EndClear chain does **not** land. Flipping the five records and regenerating fails on a stub-policy conflict, not on missing closure. The conflict is a policy decision (§3), not a tooling bug I can settle by code alone within this ticket. OPEN.
- Committed state is green: `gen_translated.py --check` on SpecGen `d14dc42b` (= merged main + EndClear records, no flips, layer untouched) reports `3953 functions (3674 recorded bit-exact, 279 closure) regenerate byte-identical`, rc=0. CONFIRMED.
- `driver.py --check` for core: NOT verified on a valid state (§4).

## 1. What `closure.py` does (commit d14dc42b)

`python3 tools/dev/translate/closure.py --image IMG --spec-set core --tag endclear --subsystem AI 0x142109EB0 0x142108630 0x14063A290 0x14063A490 0x14063D2F0`
1. Walks direct callees (CALL rel32, tail JMP to a `.pdata` function start) from each root. A callee's row comes from `tools/dev/ledger/universe.json` when present (bridge crawl kept as is); otherwise it is decoded from the image with capstone, bounded by `.pdata` (the exception directory at optional-header entry 3, read from the flat dump). No Ghidra bridge.
2. Adds the missing rows to `universe.json` (keys `%08X`, same schema, `meta.closures[tag]` records the roots and count).
3. Writes `specgen/sets/closure-<tag>.txt` (seed order), runs `specgen` pilot mode in-process, and writes each member's spec into `specs/<spec-set>/` **only if none is committed**; committed specs are kept and their difference from the generator output is printed (`--regenerate` overwrites).
4. Idempotent; `--dry-run` lists the closure; `--roots-file`.

Measured on the five EndClear roots: closure 86 members; 3 rows added (142109EB0, 142108630, 14063A290; 83 already in the universe); 12 specs in `specs/core` did not exist before (140477130, 14063DC10, 140A6CF30, 140470D00, 140A3E940, 140A57650, 14042D470, 1404F6700, 14063A420, 14063C9F0, 14159F3E0, 1415A1780). 74 committed core specs differ from the generator output: 72 by `seed` (seed = 1000 + position in the set file, so a different closure order changes it), 5 EndClear roots by hand-authored `regions`, `inputs`, `outputs`, `regions_init`, `stub_refs` (their specs were written by scratch wrappers outside the repo). Kept, not overwritten (§5).

## 2. Red (oracle), CONFIRMED

- On the merged state with the five EndClear records flipped to `diffed: true` in `results_core.json` (scratch edit, not committed), `gen_translated.py --image pes_full.bin` (6 min) fails:
  `gen_translated: 2 conflicts, first: 0x142108630 translates differently under core` rc=1.
  Log: `<workdrive>/gf-SpecGen-scratch/red/gen-red-final.log`.
- Note: main has no EndClear records, so the oracle was run on main + EndClear (the state the flips need).

## 3. Why green is not reached, CONFIRMED (root cause)

With the closure specs in `specs/core` and the five flips:
- 7 conflicts: 0x14069CD40, 0x140A760E0, 0x142108630, 0x140477130, 0x14063A490, 0x14063DC10, 0x140A57650 (`<workdrive>/gf-SpecGen-scratch/green/write2.log`).
- Without flips (closure specs only): 2 conflicts, 0x14069CD40 and 0x140A760E0 (`write3.log`).
- Variant dump (`<workdrive>/gf-SpecGen-scratch/diag/collect.log`): each conflict is two gap-free translations that differ only in how a callee is emitted:
  - `1415A1780` (sqrt): `s.set_x_f32(0, pes_model_sqrtf(...))` (a stub model) in one variant, `pes_fn_1415A1780(s)` (translated body) in the other;
  - `14159F3E0`: `/* stub: engine-noop */` in one, `pes_fn_14159F3E0(s)` in the other.
- Which variant each record uses is set by the record's own `stub_refs`. The snapdiff builds of 142108630, 14063A290 and 14063A490 (`gf-EndClear-scratch/sd5/gen/`) use the stubbed variant (5 and 3 grep hits for `pes_model_sqrtf`/`engine-noop`, 0 translated callees). 142109EB0's spec has no `stub_refs`, so its closure translates both as real calls. Its snapdiff build is not in scratch, so its stub usage is unknown (OPEN).
- The layer keeps one variant per VA, so the generator refuses. Two records of one set (and the impure set) disagree on the stub model of the same callee.
- `stubs.json` has no entry for 1415A1780, 14159F3E0, 140477130 or 14063DC10, so `stub_refs` is only a spec-level choice today.

## 4. Other checks

- `driver.py --check` (core): started on the state with the 12 extra specs; killed when they were removed from the tree mid-run, so it is not a valid result. Analysis (INF, not run): transitive `precheck.calls` over the 2253 core diffed non-snapdiff records reaches none of the 12 new VAs, so their unicorn results should not change. The gate's `diff_core_recorded` must run on Main's merge to confirm.
- No test binaries built or run in this session beyond the generator and driver. No mutation runs (no engine code changed).
- Driver change for snapdiff-proven records (skip in `--check`, since they have no unicorn corpus) was written and then reverted as unused. It is needed when the five flips land (§6, item 2).

## 5. Decisions taken

- Committed specs are kept by default (a closure run never changes a record's seed, regions or stub refs). `--regenerate` changes them; it would move 72 core records' seeds.
- The five EndClear records are not flipped on this branch. A flip needs the stub decision (§6, item 1); flipping now would break the gate.
- The bridge-free crawl is capstone + `.pdata` only; it does not do the bridge's boundary rules (RTTI engine namespaces, library symbols), so a root that reaches engine code would pull in its whole direct-call closure. `MAX_MEMBERS = 4000` guards it.
- Indirect calls are not followed (the translator's runtime-dispatch fallback covers them). Answered CutsceneHybrid2 (141F41CE0, 141F4FDA0 blocked on indirect vtable calls at 1403FB47E/1403FB49F: not a closure gap; 1403FB410's stub model is the same open policy).

## 6. Open items (numbered)

1. **Stub policy (blocks the EndClear chain).** Pick one: (a) a callee has one stub model across the layer, taken from one table (stubs.json), with `stub_refs` derived from it; the five records' stubs then need re-proof, or (b) keep per-record stubs and let each set's closure use its own variant (gen would emit per-set copies). Owner/Main decision.
2. Land the five flips with `"proof": "snapdiff"` and their replay counts (14063D2F0 12/12; 142109EB0 5/5 danger-0 arm only; 142108630, 14063A290, 14063A490 6/6 each), plus the driver `--check` skip for them. Then run `closure.py` and `gen_translated.py`; then `--check`.
3. Regenerate the 74 kept core specs or keep them (decide; affects seeds).
4. Run `diff_core_recorded` on the merge (not run here, §4).
5. Missing universe rows for 142109EB0, 142108630, 14063A290 come from the closure run (not committed yet, by design, since the run needs item 1).

## 7. Commands

- Closure: `"$GF_DIFF_PYTHON" tools/dev/translate/closure.py --image "$PES_EXE" --spec-set core --tag endclear --subsystem AI 0x142109EB0 0x142108630 0x14063A290 0x14063A490 0x14063D2F0`
- Regenerate: `"$GF_DIFF_PYTHON" tools/dev/translate/gen_translated.py --image "$PES_EXE"` (writes `src/pes/translated`); drift: `... --check`.
- Scratch: `<workdrive>/gf-SpecGen-scratch/` (red, green, diag, check-core logs). Nothing written to `<workdrive>/GodotFootball`.


## Main's ruling (28-10-26): one global stub table

A callee is either always a model or always its translated body, in every set: CRT and engine routines (`sqrtf` 0x1415A1780, `__security_check_cookie` 0x14159F3E0, imports) are always models; PES's own functions are always translated. One translation per function follows, which the layer requires.
