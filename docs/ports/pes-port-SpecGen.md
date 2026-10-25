# pes-port-SpecGen: one spec generator, the reproduction check, and the cutscene rollout (25-10-26)

Branch `SpecGen`, head `defa161` (ls-remote confirmed). Commits: `0014bdd` (generator, WIP), `d2a17ae` (switched specs and
recorded results), `defa161` (merge of origin/main `4be8794`, batch Z). Gate not run (Main gates). All runs: scratch in
`<workdrive>/gf-SpecGen-scratch`, `ulimit -v 2000000`, unicorn 2.1.0, capstone 5.0.7, image `pes_full.bin`, `--jobs 2`.

## Result

- **Generator in the repo:** `tools/dev/diffharness/specgen/`, run as `python3 -m specgen select|generate` from
  `tools/dev/diffharness`. It replaces the two scratch pipelines (`<workdrive>/gf-Pilot-scratch/map`, `<workdrive>/gf-Impure-scratch`);
  nothing in it refers to a machine path.
- **Pilot (252):** the regenerated specs give the same 183 bit-exact functions as the committed specs. Nothing lost, nothing gained.
- **Impure (50):** 7 bit-exact committed, 8 regenerated (`0x140639100` gained, none lost).
- **Cutscene (751 pure seeds + 82 pure callees = 833):** 830 translated, 509 bit-exact at the 50% floor.
  751 seeds: 447 bit-exact (59.5%). Decision rule (plan, 30-70% band): fix the top failure classes in the translator and
  struct map, then re-pilot. Not a roll-out yet.

## Reproduction check (CONFIRMED: driver runs)

| set | committed specs, full run | regenerated specs, full run | same bit-exact set | spec content (committed vs regenerated) |
|---|---|---|---|---|
| pilot, 252 | 252 translated, 183 diffed | 252 translated, 183 diffed | yes, no loss | 247 identical except `note` text; 5 differ only in the `domain` string of one case-varied input |
| impure, 50 | 46 translated, 7 diffed | 46 translated, 8 diffed | 7 kept, `0x140639100` gained | see classes below |

Classes of difference, impure (50):
1. `note` and `closure_fix.rule` text (all 48 regenerated): the note names `rules.closure_fix`, not `fixclosure.py`.
2. Fifth G accessor `0x140A489F0` (base 0xF3, count 22; bb2c491 applied it to one spec only): 19 specs reach it and gain
   its 22 slots in `closure_fix`. In 17 the slots enter `regions_init` (E1EE declared). In 2 (`1404FB2A0`, `14097CD00`)
   they are dangling (E1EE undeclared) and are dropped, so their `regions_init` is unchanged.
3. Not generated: the G+0x180 table relink (`14065C2A0`, `140978E70`, `IndirectResolve`). These two keep their
   committed hand-made specs. Their regenerated versions change class from `harness cannot run` to `struct map gap`; both are
   not bit-exact either way.
4. `14053B420`: the fifth accessor is listed in a different order only. Its content is unchanged.

Pilot differences are all text: `note` (generator provenance) and the `domain` strings of `G_14351AE54` (3 specs),
`G_143704E38` (1) and `G_14351AE50` (1). Inputs, regions, outputs (5484011 audit), `live_data_reads` and `patch_addrs`
match the committed specs in all 252 specs.

Switch decision: pilot specs switched (regenerated, same results). Impure specs switched except the two in item 3 (8 ≥ 7,
no loss). `results.json` and `results_impure.json` re-recorded from full runs of the switched dirs.

## Hand fixes carried as rules (`rules.py`, provenance in each docstring)

- `case_varied_globals` (pilot mode, 95f986f): five named functions read `.data` globals as case-varied inputs.
- `audit_outputs` (pilot mode, 5484011): every return register written is checked (`translate/spec_outputs.py`).
- `closure_fix` (impure mode, bb2c491, fixclosure.py): G record accessors laid out from the accessor's code.
  Base, count and fallback are read from bytes (`cmp edx,n / add rax,base / mov rax,[rcx+fb]`); the shape is checked,
  and an unexpected shape raises. The blanket 234 E1EE slots on G are removed. G child links are restored (CONFIRMED,
  disassembly of `140A489A0`, `140A489F0`, `140A48A30`).
- `drop_dangling` (impure mode, bb2c491): `regions_init` entries naming undeclared regions are dropped.
- Negative indexed offsets (`IDX+0x-…`) parse (gen/gen_specs.py fix; needed for `impure`).
- Seeds: `1000 + position` in the set file. Set files (`sets/*.txt`) keep the source order.

## Cutscene rollout (driver full run, recorded in `tools/dev/translate/results_cutscene.json`)

| set | attempted | translated | bit-exact at floor | bit-exact below floor | own-block coverage median (quartiles) |
|---|---|---|---|---|---|
| 751 cutscene seeds | 751 | 748 | 447 | 48 | 0.89 (0.00 / 0.89 / 1.00) |
| 82 pure callees | 82 | 82 | 62 | 7 | 1.00 (0.65 / 1.00 / 1.00) |
| all 833 | 833 | 830 | 509 | 55 | 0.90 (0.00 / 0.90 / 1.00) |

Seeds are the ledger's `pure_rows` with subsystem `cutscene`: 751 (the brief's count). Callees are the closure over pure
callees: AI 25, in-match menu 16, restart 9, pre-match 7, ball 6, gameplay 4, other 15.

Coverage distribution, 830 translated functions:

| own-block coverage | bit-exact at floor | not bit-exact |
|---|---|---|
| < 25% | 0 | 226 |
| 25-50% | 0 | 42 |
| 50-75% | 79 | 7 |
| >= 75% | 430 | 46 |

Failures by class (324 not bit-exact):

| class | count | examples (VA) | what the note says |
|---|---|---|---|
| struct map gap | 209 | 0x14000B250, 0x140012F50, 0x14001A6C0 (UC_ERR_MAP); 0x1402A51C0, 0x1402A51E0, 0x1403FB710 | 206 unicorn "Invalid memory mapping" faults; 1 region overlap (0x1414BB950, generator region size); 1 CPU exception (0x14175850F); 1 harness ValueError on a negative pointer-path element (0x141F79FF0) |
| translator bug | 60 | 51 bit mismatches: 0x140001750 (`w_A0_10_d0`), 0x140003710 (`w_A0_30_d0`), 0x140004400 (`w_A0_0`), 0x140004DC0 (`w_A1_3C`), 0x14002A970 (`ret_xmm0`); 6 timeouts after 600 s: 0x1408D10B0, 0x1408D1410, 0x141F4C880; 3 translate TypeErrors (not translated): 0x140005530, 0x1400DB970, 0x140104DA0 | mismatch = output differs at the named case |
| harness cannot run | 55 | 0x14000AEF0, 0x140019030, 0x140020910 | bit-exact but own-block coverage below the 50% floor |

Not produced by the driver: engine call, live state, GPU. The fault classifier put every fault in synthetic space
(struct map gap). Whether any of the 206 map faults are reads of image `.data` (live state) is not diagnosed (INF).

## Requests spent per stage (approximate; about 115 tool calls in total)

- Orientation (brief, AGENTS, plan, docs, scratch scripts, data checks): ~38
- Generator build (driver `--record`, package modules, consolidation, set files): ~30
- Reproduction (diffs, scoping the case-varied rule, the negative-offset fix, accessor analysis): ~17
- Cutscene (select, generate, runs, summaries, failure tables): ~9
- Gate, README, commits, merge of origin/main, merged pilot check, push, this report: ~18

## Verification run (all scratch output unless noted)

- Baseline pilot (committed specs): 252/252 translated, 183 diffed (matches the committed record).
- Regenerated pilot: 183 diffed, same set. Impure committed 7, regenerated 8. Cutscene full run: 833/830/509.
- Recorded full runs in the repo: pilot 252/252/183, impure 50/46/8, cutscene 833/830/509.
- `driver.py --check` pilot: 183/183 bit-exact (before the merge and after it, rc 0).
- `driver.py --check` cutscene: 509/509 bit-exact (rc 0; run before the merge).
- The merge brought batch Z changes: `fieldmap.json` (`gf` strings, never read by the translator), `pes_state.hpp`
  (the window path, inactive unless set), and a new team-sheet test. The merged pilot check is green.
- PII: no machine paths in source or specs (grep). `.pyc` caches were removed before staging.
- Gate counts: `diff_pilot_recorded` = driver `--check` (183). New `diff_cutscene_recorded` (509).

## Open items

1. The G+0x180 table relink is not generated. `14065C2A0` and `140978E70` stay hand-made (both not bit-exact).
2. Cutscene specs use pilot mode, which keeps the blanket E1EE slots on G. The accessor layout and blanket removal
   exist only in impure mode. Their effect on the 206 map faults is untested (INF).
3. The five case-varied inputs' `domain` strings differ in text from the committed ones.
4. Generator bugs seen in the cutscene set: `0x1414BB950` (regions A0 and A1 overlap: a region's size comes from its
   field offsets, and a large one passes the 4 MB stride) and `0x141F79FF0` (`objname` in `spec.py` parses a negative
   pointer-path element with `int(p, 16)`; only indexed offsets were fixed).
5. Not diagnosed: the 206 map faults, the 3 translate TypeErrors, the 6 timeouts, and `0x14175850F` (UC_ERR_EXCEPTION).
6. `stubs.json` and the impure sample's quota logic (`pick.py`, `emit.py`) are not regenerated. The impure VA list is
   committed in `sets/impure.txt` (picks order).
7. The decision rule puts cutscene in the 30-70% band (59.5%). Next: fix the top classes (map faults, mismatches) and re-pilot.
8. Live PES reads were not used. The specs take their values from the image only.


## Main at landing

`results_cutscene.json` was left uncommitted although `diff_cutscene_recorded` reads it; Main committed it from SpecGen's full run (`c418423`). Batch AA gate: 2033/2033, `diff_pilot_recorded` and `diff_cutscene_recorded` both passed.
