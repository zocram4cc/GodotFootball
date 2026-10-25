# PES impure-sample map: 50 functions, callee stub models, specs (ImpureMap)

Static RE only (no live PES). Deliverables: `<workdrive>/gf-Impure-scratch/` = `sample.json`, `stubs.json`, `specs/<VA>.json` (50), `BATCH1.txt`. Scratch scripts: `build.py`, `finalize.py`, `emit.py`, `gen/gen_specs.py` (copy of the pilot generator; one change: negative field offsets accepted). No repo edits.

## 1. Sample (50)

Rule: primary class per pool row (vtable > indirect > engine > impure-callee). Quotas 30 engine / 15 impure-callee / 4 indirect / 1 vtable (the pool holds 259 / 126 / 27 / 2 rows on that rule). Within each class, the pool sorted by size, evenly spaced across size bins; in each bin a function with a GF port (VA present in `src/`) preferred. 11 of 50 have a GF port (the pool has few; the port check is a token match on `0x<VA>` in `src/`).

| VA | stratum | size | size q | GF port | out-of-set callees | stubs | harness class |
|---|---|---|---|---|---|---|---|
| 140A57650 | engine | 47 | 0.01 | yes | 1 | 1415A1780 | runnable with stubs |
| 14080EBA0 | engine | 67 | 0.05 | - | 1 | - | harness cannot run |
| 1404F47A0 | engine | 88 | 0.08 | - | 2 | 1415A1780 | runnable with stubs |
| 14097FCE0 | engine | 103 | 0.11 | - | 2 | 14159F3E0 | runnable with stubs |
| 14064DC50 | engine | 124 | 0.15 | - | 1 | 1415A1780 | runnable with stubs |
| 140529490 | engine | 139 | 0.18 | - | 2 | 1415A1780 | runnable with stubs |
| 14053B420 | engine | 160 | 0.21 | - | 1 | 14159F3E0 | runnable with stubs |
| 1405055E0 | engine | 170 | 0.25 | - | 3 | 1415A1780 | runnable with stubs |
| 140510D30 | engine | 185 | 0.28 | - | 5 | 14159F3E0 | runnable with stubs |
| 140A539F0 | engine | 194 | 0.31 | - | 2 | 1415A1780 | runnable with stubs |
| 14097FFF0 | engine | 220 | 0.35 | - | 1 | 1415A1780 | runnable with stubs |
| 14065C2A0 | engine | 239 | 0.38 | - | 2 | 14159F3E0 | runnable with stubs |
| 1405A6DE0 | engine | 260 | 0.41 | - | 5 | 1415A1780 | runnable with stubs |
| 140639100 | engine | 286 | 0.45 | - | 2 | 1415A1780 | runnable with stubs |
| 1405CE4F0 | engine | 290 | 0.46 | yes | 12 | 14159F3E0,1415A1780 | harness cannot run |
| 140687310 | engine | 318 | 0.51 | - | 1 | 14159F3E0 | runnable with stubs |
| 14065DAB0 | engine | 345 | 0.54 | - | 4 | 1415A1780 | runnable with stubs |
| 14064DE60 | engine | 378 | 0.58 | - | 4 | 1415A1780 | runnable with stubs |
| 140654E60 | engine | 440 | 0.63 | yes | 1 | 1415A1780 | runnable with stubs |
| 14051B8B0 | engine | 444 | 0.64 | yes | 4 | 14159F3E0 | harness cannot run |
| 140975710 | engine | 509 | 0.68 | - | 5 | 14159F3E0,1415A1780 | harness cannot run |
| 1404FEDC0 | engine | 524 | 0.70 | yes | 5 | 14159F3E0,1415A1780 | harness cannot run |
| 1405CE930 | engine | 585 | 0.74 | yes | 5 | 14159F3E0 | harness cannot run |
| 14062E240 | engine | 651 | 0.78 | - | 6 | 14159F3E0,1415A1780 | harness cannot run |
| 140503430 | engine | 750 | 0.82 | yes | 2 | 1415A1780 | runnable with stubs |
| 1404F6C30 | engine | 772 | 0.83 | yes | 60 | 14159F3E0,1415A1780 | harness cannot run |
| 1406486D0 | engine | 930 | 0.88 | - | 8 | 1415A1780 | runnable with stubs |
| 1404F8C70 | engine | 1031 | 0.92 | - | 7 | 14159F3E0,1415A1780 | harness cannot run |
| 14052A820 | engine | 1272 | 0.95 | - | 7 | 1415A1780 | runnable with stubs |
| 1405DC350 | engine | 2102 | 0.98 | - | 16 | 14159F3E0,1415A1780 | harness cannot run |
| 14057BA50 | impure-callee | 40 | 0.03 | - | 2 | - | runnable (closure real) |
| 1404FB2A0 | impure-callee | 61 | 0.11 | yes | 3 | 1415A1780 | harness cannot run |
| 1404FA720 | impure-callee | 69 | 0.16 | - | 5 | - | harness cannot run |
| 14053B110 | impure-callee | 83 | 0.23 | - | 8 | 14159F3E0 | runnable with stubs |
| 14051CF70 | impure-callee | 95 | 0.29 | - | 2 | 14159F3E0 | runnable with stubs |
| 1405CCD50 | impure-callee | 112 | 0.35 | yes | 1 | 1415A1780 | runnable with stubs |
| 14052A190 | impure-callee | 129 | 0.42 | - | 3 | 14159F3E0 | harness cannot run |
| 14097CD00 | impure-callee | 149 | 0.49 | - | 4 | 1415A1780 | runnable with stubs |
| 140978E70 | impure-callee | 175 | 0.56 | - | 2 | 1415A1780 | runnable with stubs |
| 140509A40 | impure-callee | 203 | 0.63 | - | 12 | - | harness cannot run |
| 14057D3A0 | impure-callee | 233 | 0.70 | - | 8 | 1415A1780 | runnable with stubs |
| 1404FE9C0 | impure-callee | 268 | 0.78 | yes | 5 | 1415A1780 | harness cannot run |
| 1405DE950 | impure-callee | 322 | 0.82 | - | 1 | 14159F3E0,1415A1780 | harness cannot run |
| 1404405D0 | impure-callee | 432 | 0.90 | - | 18 | 14159F3E0,1415A1780 | harness cannot run |
| 1404386D0 | impure-callee | 563 | 0.96 | - | 10 | 14159F3E0,1415A1780 | harness cannot run |
| 14057A3E0 | indirect | 88 | 0.11 | - | 2 | 14159F3E0 | harness cannot run |
| 140601110 | indirect | 192 | 0.33 | - | 0 | - | harness cannot run |
| 140504D20 | indirect | 302 | 0.59 | - | 16 | 14159F3E0,1415A1780 | harness cannot run |
| 1405E4460 | indirect | 635 | 0.85 | - | 5 | 14159F3E0,1415A1780 | harness cannot run |
| 1405AD720 | vtable | 311 | 0.50 | - | 7 | 14159F3E0,1415A1780 | harness cannot run |

Harness class totals: runnable with stubs 26; runnable (closure real) 1; harness cannot run 23. "Runnable" here is a static closure verdict, not a harness result: nothing has been executed under unicorn.

## 2. Callees (234 out-of-set direct callees of the 50; union = pool's, no overlap with the pilot set)

Pilot set = `pilot-ai-set.json` (252). The 234 are exactly the pool's non-pilot callee union.

| kind | count | meaning |
|---|---|---|
| return-input | 1 | 1415A1780, sqrt (modelled) |
| engine-noop | 1 | 14159F3E0, `__security_check_cookie` (modelled) |
| run-real | 114 | no call, no indirect site, no global store (control-flow walk). The harness executes the image bytes; the C++ build must translate the VA (pure extension) |
| run-real-closure | 33 | executes image bytes; its closure calls 1415A1780 or 14159F3E0 |
| unmodellable | 85 | blocked; reasons below |

Unmodellable 85, by first blocking reason: indirect call site 60; computed jump whose case bodies are not enumerated 19; import slot with no name 4; logf/powf thunk 1; global store (live state) 1.

Modelled entries (149 = 114 + 33 + 2) carry `model` and `why`; unmodellable entries carry `model: null` and the reason.

### Named models (evidence)

- **1415A1780 → sqrtf, return-input.** Thunk `jmp [0x1425301F0]`. Name from `docs/re/pes-re-carrier-helpers.md` table (FUN_1415A1780 = sqrt) and `docs/re/pes-re-lane-core.md` O3. IEEE sqrt is correctly rounded, so C++ `std::sqrt(float)` gives the same bits. CONFIRMED by doc, INF on the bit-exactness claim until diffed.
- **14159F3E0 → __security_check_cookie, engine-noop.** Ghidra name at the VA (`get_function_by_address`); docs decoded row. Effect on the success path: none. Alternative: the harness could run it for real, since prologue and check read the same mapped global; the model is a choice, noted.
- **1415A17C8 / 1415A17CE → logf/powf (unmodellable).** Docs name them as imports; result bits come from the C runtime, outside the image.

### Unmodellable, what each means

- 60 indirect call sites (`call [reg+off]` on heap objects or tables). The target is not named by the image or by docs. Examples: the indirect picks 14057A3E0 (vtbl+0x20 release), 140601110, 140504D20, 1405E4460 (table at +0x344), and the vtable pick 1405AD720 (slot +0x10). Docs describe these functions but not their indirect targets, so no target was resolved.
- 19 computed jumps: switch tables whose case bodies were not decoded. Their closure is incomplete; the harness would run them but the closure cannot be proven.
- 4 import slots with no name in the image. The PE import directory in the dump does not match the IAT the thunks jump through (thunks at 0x1415A1xxx reference 0x1425301xx). Ghidra `list_imports` is not on the bridge. Only the docs-named imports are modelled.
- 1 global store (live state) and 1 logf/powf thunk.

Indirect and vtable picks: none resolved. Every one is `harness cannot run`.

## 3. Specs

`specs/<VA>.json` (50), pilot-spec format, generated from the image by the pilot analyzer and generator (unchanged analyzer; generator copy patched for negative offsets). Added fields: `stub_refs`, `harness` (class, blockers, out-of-set callees, indirect sites, switch jumps). Note: the pilot analysis walks callees with the pool's instruction counts; run-real callees use control-flow instruction counts (estimates).

Batch 1 (10 specs, runnable with stubs; sent to ImpureRun and ImpureRun-2): 140A57650 1404F47A0 14097FCE0 14064DC50 140529490 14053B420 1405055E0 140510D30 140A539F0 14097FFF0. Their stubs: 1415A1780 and 14159F3E0. Specs were NOT run through `run.py --check-spec` or the driver.

## 4. Method notes and ceilings

- Callee sets for the 50 are the pool's ledger walk (authoritative). A control-flow walk (branches within 0x10000 after entry and 0x2000 before) agreed on 47 of 50; the 3 differences come from switch targets (not followed) and from over-inclusion of neighbouring code through branch windows. Nested closures use the control-flow walk, so they are conservative in both directions.
- `stubs.json` is a list (contract: va, kind, why, model; plus callers and used_by_closure). Reason strings are sanitised: no operand text, only VAs.
- No PES bytes committed: stub models cite VAs and docs; no disassembly in the deliverables.

## 5. Cost

About 80 tool requests for the whole task. Named models (sqrt, cookie, logf/powf, plus the import-directory attempt): about 6 requests. The other 232 entries and the 50 specs came from one classification script and one generator run: a handful of requests total, not per stub.

## 6. Checks Main should run

- `run.py --check-spec` on batch 1 and on all 50 (the probe must accept the stubs: the harness has no kinds other than `do: return` yet; `return-input` and `engine-noop` need ImpureRun's extension).
- Translate and build the run-real closure VAs (114 + those in the 10 batch-1 closures) before any batch-1 diff; the C++ build currently treats them as engine boundaries.
- Re-verify the sqrt bit-exact claim (diff one sqrt caller) and the cookie no-op choice.
- Open items: resolve the 4 unnamed import slots (Ghidra list_imports or docs); enumerate the 19 switch-table case bodies; resolve the vtable/indirect targets for the 5 indirect and vtable picks (needs heap-object class docs).
