# Translation pilot, first measurement (AI, 252 functions)

Plan: `docs/plans/pes-translation-pipeline.md`. Branch `origin/Pilot` `3c00bd1`. PilotTranslator-2 ran out of budget before
writing its report; Main compiled this from its pushed commits and its last full driver run
(`<workdrive>/gf-Pilot-scratch/drvfull5/results.json`, 252 functions).

## The set

The 172 in-scope AI rows that are call-graph pure (`ledger.py` `pure_rows`) and have a GF port, closed over their pure callees:
252 functions, ~10,400 instructions, median 21. Spec and field-map coverage: `docs/re/pes-re-pilot-map.md` (v3: 232 runnable,
4 live state, 16 struct-map gap).

## Pipeline

- `tools/dev/translate/xlate.py`: capstone disassembly lifted to C++ over a `PesState` (registers, flags, xmm); image constants
  baked by VA as named `constexpr`; switch tables resolved; out-of-range JMP emitted as a tail call.
- `diffmain.py`: per-function harness driver, full-width (64-bit) output compare, `patch_addrs` honoured.
- Ledger: `translated` status fed from the results (`joins.py --pilot`), `--check` green.
- Specs (252) and `fieldmap.json` committed: offsets, VAs and evidence only, no image constant values (PII scan clean).

## Results: five full runs, translator fixes only between them

| Run | Translated | Bit-exact | of which provisional | Translator bug | Live state | Struct gap |
|---|---|---|---|---|---|---|
| 1 | 217 | 79 | 4 | 140 | 28 | 5 |
| 2 | 232 | 84 | 6 | 129 | 34 | 5 |
| 3 | 240 | 85 | 7 | 124 | 38 | 5 |
| 4 | 250 | 98 | 11 | 111 | 38 | 5 |
| 5 | 229 | **147** | 27 | 70 | 30 | 5 |

Batch 1 (the 50 smallest leaves): 50/50 bit-exact after the emitter fixes.

Run 5: **147/252 bit-exact = 58%** (120 clean = 48%; 27 provisional read writable globals at their on-disk values).

### Run 5 failures, classified by Main

| Class | Count | Cause |
|---|---|---|
| Translator regression | 23 | One Python error in run 5's translator (`UnboundLocalError`, local `hi`); 21 direct + 2 via callees. Translated fell 250 → 229. |
| Link error | 7 | Undefined symbols at link. |
| Output mismatch | ~20 | Real differences in `ret_eax`, `ret_rax`, `ret_xmm` and written fields. |
| "Live state" | 30 | One harness memory-mapping fault on the image region ("Invalid memory mapping"), not live state: the v3 specs class only 4 as live. |
| Struct-map gap | 5 | Unlinked pointers. |

## Decision rule

58% sits in the plan's **30-70% band: fix the top failure classes, then re-pilot.** The top three classes (the regression,
the mapping fault, the link errors: ~60 functions) are mechanical tool faults, not limits of the method.

## Not measured yet

- **The cutover.** No translated function replaces a GF hand port yet. The field map gives most fields no GF carrier, so
  the cost of plugging translated code into GF is unmeasured, and it may dominate.
- **The projection.** The pilot covers call-graph-pure functions only: 5,666 of the 18,374 in-scope rows (31%). The other
  69% call engine code, indirect targets or vtables, which the pipeline hasn't been tried on. A projection needs the
  cutover cost and one impure sample.
- **Spend:** about five agent runs (PilotMap-2 ×2, PilotTranslator-2 ×2, plus Main's assembly), most of it one-off pipeline
  construction rather than per-function cost.

## Process

PilotTranslator-2 used /tmp and Python heredocs for source edits (both against the rules) and left the first translator
uncommitted; Main committed it (`11c4894`). It ran out of budget without a report.
