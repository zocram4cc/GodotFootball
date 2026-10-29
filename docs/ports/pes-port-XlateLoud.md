# PES port: no silent translations: loud tail jumps, closure follows them, logf/powf models (XlateLoud, 29-10-26)

Branch `NativeSeam` (worktree `<workdrive>/gf-NativeSeam`), head `970a8d47` (confirmed by `ls-remote`), `origin/main` 9bf645e8 merged first (symlink trap: `git status` fails in the worktree, `git merge` and `ls-files -m` work). Labels: CONFIRMED = read in tool output; INF = inferred; OPEN = blocker named. Gate not run (Main gates); fast-tier line at the end.

## 0. Summary

- **No silent return remains.** xlate no longer emits `/*OUT-OF-RANGE JMP t*/return;`. A `jmp t` past the function's body to a target not in the universe is `pes_dispatch_missing(t); return;` (aborts naming the VA, like an out-of-set CALL; the `return;` stays a separate line because `flow.py` reads it as the block terminator).
- **The closure walk follows tail jumps (the preferred rule).** `driver.runnable_extra(spec, image, universe=...)` now walks the spec's function and every universe member it reaches; a `jmp t` past a body whose `t` is not in the universe becomes an out-of-set seed (`tail_rows`), exactly like an out-of-set callee. The three places that build a universe (`gen_translated.context`, `driver.build_and_diff`, the translate step, the coverage step) pass it, so the layer and the diff proofs use one universe.
- **A generation-time check refuses what is left.** `gen_translated.silent_tail_sites(members)` fails the whole generation, naming the file and the target, for an `OUT-OF-RANGE JMP` whose target is inside the image's code range (`driver.CODE_LO..CODE_HI`, now named once).
- **Layer regenerated**: 4,588 functions (3,683 recorded, 905 closure; was 4,011 / 328): +577 closure members, 13 changed files (+ `translated.hpp`, `pes_dispatch_gen.hpp`). `gen_translated.py --check`: **4588 functions regenerate byte-identical** (CONFIRMED, tool output).
- **Re-run of every recorded function whose file or closure changed (13): verdicts unchanged** after one harness fix (§4).
- **`logf` and `powf` are direct-callee models** (`MODEL_DOS`, xlate, run.py, `pes_stubs.hpp`), tested against C99 Annex F and against the game's own ucrtbase code. Special values are bit-identical to ucrtbase; **ordinary arguments are not: they differ by one ulp in 0.35% of logf and 0.15% of powf inputs** (§3).

## 1. The red test (oracle) and the rules

`tools/dev/translate/test_tail_jump_loud.py` (9 tests; ctest `translate_tail_jump_loud`, skips 77 without `GF_DIFF_PYTHON`) builds a synthetic flat image (offset = VA - 0x140000000): `FN: xor eax,eax; jmp .+2; call CALLEE; jmp TAIL`, `TAIL: jmp TAIL2`, `TAIL2: ret`, targets past the decode window.

| test | on `origin/main` | now |
|---|---|---|
| untranslated tail target aborts naming the VA | **FAIL** (the translation held `/*OUT-OF-RANGE JMP*/return;`, no abort; CONFIRMED run before the fix) | pass |
| translated tail target is `{ pes_fn_T(s); return; }` | pass | pass |
| `runnable_extra(spec, img, universe=)` pulls TAIL and TAIL's own tail TAIL2 | **ERROR** (no `universe` argument) | pass |
| a universe member's own out-of-universe tail target is pulled in | ERROR | pass |
| target already in the universe is not extra; local jumps and out-of-universe CALLs are not rows | ERROR | pass |
| layer check names an in-code untranslated tail site; tolerates decode noise outside the code range | ERROR (no function) | pass |
| `choose_variant`: fewer gaps wins in either order; a tie that differs is a conflict | n/a (new) | pass |

**Mutations (run on a scratch copy of the tool tree, each killed; first run T7 survived, the test gained a CALL in `FN`, then it died):** T1 silent return restored; T2 no tail walk; T3 layer check off; T4 layer check without the code-range bound; T5 universe members not followed; T6 local jumps become rows; T7 CALLs become rows; T8 in-universe tail not called; V1 always keep first variant; V2 always take newer; V3 a tie keeps old (no conflict); V4 prefer more gaps. **12/12 killed.**

## 2. What the 16 remaining targets were (CONFIRMED: grep of the layer before and after; PE section table of `pes_full.bin`)

| target | verdict |
|---|---|
| `1415A17CE` (powf thunk, 2 sites: 14231E894, 14231E964) | now the powf model (§3) |
| `1403D2840`, `141FEE330`, `141FFB990`, `141FFB9D0`, `141B37CE0`, `141B56780`, `141B59850` | in `.trace`: pulled into the closure; their sites are `pes_fn_T(s); return;` |
| `143EF8930`, `143F667C0`, `143F6A7F0`, `14417E440`, `14428B510` (the five thunk-only closure members `14043A7E0`, `14053A8A0`, `14053ACE0`, `140A426F0`, `140A7C640`) | **OPEN, see below**: outside `.trace` |
| `14417D0C0` (140A3F770, 140A3F730), `144ADC120`, `15B23CA30` | outside `.trace`; sites follow `return;` and INT3 padding: the next function's thunk decoded as this body's tail (INF: dead) |

**OPEN 1 (a real gap the check does not see).** The five thunk-only members are real tail calls into **`.impdata`** (`0x143D14000`, size `0x17C3D94F`, flags `0xE0000020`): the bytes at `143EF8930` (`48 89 C8 45 84 C0 74 0C ...`) and `14417D0C0` (`48 89 5C 24 08 48 89 6C 24 10 ...`) are real function prologues. Ghidra's bridge knows no function there, and `driver.CODE_LO/HI` (= the ledger universe) is `.trace` only. They are now loud aborts (before: silent returns), and `silent_tail_sites` tolerates them because it bounds on `.trace`. Closing it means widening the code range to the executable sections that hold code, then translating that code; it was not attempted (the size of the `.impdata` closure is unmeasured, and it needs its own ledger rows). The decode-noise sites in the table above (and the `14037FCD0..` / `1403AB2D0..` families: 41 sites in all, every one after `return;`/INT3 or the whole body of a thunk) stay loud aborts by design.

## 3. libm models `logf` and `powf` (CONFIRMED by script over `live2/pid1694737/seq009`)

- **Mechanism**: `stubs.json` entries `1415A17C8` (new, kind `libm`, op `logf`) and `1415A17CE` (powf); `stubs.py` `MODEL_DOS` gains `logf_xmm0`, `powf_xmm0`; `xlate.stub_lines` emits `s.set_x_f32(0, pes_model_logf(x0))` / `pes_model_powf(x0, x1)` (a tail `jmp` to the thunk is the model plus `return;`); `run.py` hooks `logf_xmm0`/`powf_xmm0` (Python models over the host libm, bits 32-127 of xmm0 kept as for sqrt); `pes_stubs.hpp` has `pes_model_logf/powf` (`std::log`, `std::pow` on the host libm). One choice for every spec set, as the stub ruling says.
- **The slot values are code in the snapshot's ucrtbase** (a native Microsoft build: `cmp dword [rip+..],0` ISA dispatch, MSVC idioms): `0x6FFFFEC7D5E0` (slot `0x142530278`) returns `logf(2)=0x3F317218`, `logf(0)=-inf`, `logf(-1)=0xFFC00000`; `0x6FFFFEC7F090` (slot `0x142530290`) returns `powf(2,128)=inf`, `powf(-8,1/3)=0xFFC00000`. That is the evidence for the names (before: only a docs naming in `indirect.py`).
- **Test** `tools/dev/translate/test_libm_models.py` (5 tests; ctest `translate_libm_models`; the ucrtbase half runs when `PES_UCRT_SNAP` names a full snapshot): (1) Annex F results, expectations from the standard (log: ±0, 1, negatives, ±inf, NaN; pow: the 47-row table of F.9.7.4 including odd-integer and ±0/±inf cases, plus overflow `2^128`, underflow `2^-150`, `2^-149` = the smallest denormal); (2) the C++ model (built with g++ from the real header) and the Python model agree on 8,000+ inputs; (3) **the ucrtbase machine code run in unicorn** (module mapped from the snapshot; calls out of the module land on a `xor eax,eax; ret` page): **every special value (zeros, negatives, NaN, ±inf, denormal arguments, exact powers, overflow/underflow) is bit-identical, NaN signs included**; ordinary arguments are within one ulp.
- **Where they differ (the answer to "say if they do"): yes.** Fixed sample (seeded; 4,000 random bit patterns, 4,000 in 0..100 and 0.5..2): **logf 28 of 8,013 inputs (0.35%) and powf 10 of 6,551 (0.15%) differ by one ulp** from ucrtbase (ucrtbase is double-precision MS code, glibc's logf/powf are correctly rounded more often). One class of powf results is not a rounding wobble: for results next to the smallest denormal (7 of 313 denormal-result samples), ucrtbase returns 0 where the host gives `0x00000001`. The model keeps the host libm and says so (`// ponytail:` in `pes_stubs.hpp`, `caveat` in `stubs.json`). The removal is ucrtbase's own algorithm and tables, read at run time from the user's ucrtbase module; not done (the tables live in a DLL outside the PES install, so it needs an owner decision on where that module comes from).
- **Mutations** (killed): L1 powf args swapped, L2 logf is log2, L3 powf zero base flushed, L4 logf of a negative returns -inf, L5 Python logf model different, L6 Python powf args swapped. **6/6.**
- **Other libm imports the recorded/hooked closures reach** (script over every spec's `out_of_set_callees` in the 1415A1xxx thunk page + grep of the layer): only `1415A1780` (sqrt, already a model) and `1415A17CE` (powf). `1415A17C8` (logf) is reached by no spec yet (xrefs show 20+ engine callers); `1415A17BC` (slot `0x142530260`, a double function: `x - f(x)` then `cvttsd2si`, INF floor) is reached by recorded `1403D2E60` and stays an out-of-set abort. **OPEN 2:** name `1415A17BC` (and the other ~100 thunks on that page) the same way.

## 4. Verdicts after the regeneration (CONFIRMED, driver output)

Set of re-run: every recorded function whose own file changed (8: 1403D2E60, 140A3F730, 140A3F770, 1415184E0, 141B50460, 141FEE390, 14231E894, 14231E964) plus every recorded function whose layer closure reaches a changed or new file (5 more: 1404386D0, 1404405D0, 1404FB2A0, 14053B110, 140A7CC00). Run with `driver.py --only ...` on the right spec dir (cutscene, presentation, core, impure, fixdemo).

| function | set | result |
|---|---|---|
| 1404405D0, 1404FB2A0, 14053B110 | impure | bit-exact (128 / 78 / 117 cases), as recorded |
| 140A3F770 | cutscene | bit-exact, 124 cases |
| 141B50460, 14231E894, 14231E964 | presentation | bit-exact (64 / 54 / 50 cases; the two powf functions diff with the new model on both sides) |
| 140A3F730, 1415184E0 | core | bit-exact (75 / 74) |
| 140A7CC00 | fixdemo | bit-exact, 68 |
| 1403D2E60 (cutscene), 141FEE390 (core) | | **first run: `translator bug`, note `compile: ` (no error line).** Cause (CONFIRMED): the driver's single `g++` over every TU had a hard 300 s timeout; the tail closure made these builds 352 / 598 TUs (before: a handful), the compile timed out, and `sh()` returns rc 124 with no `error` line. Not a translation fault: every `pes_fn_*.cpp` of the 1403D2E60 build passes `g++ -fsyntax-only`. Fix: `timeout = 300 + 20 * len(objs)` (`ponytail:` in `driver.py`; parallel objects is the upgrade). **Second run: both bit-exact (69 / 64 cases).** |
| 1404386D0 | impure | not re-diffed: its recorded corpus is a capture bank (`GF_IMPURE_BANKS`, not available here); the run without it reports `harness cannot run` exactly as the committed check does without the bank. Its layer closure changed, so it stays unverified until Main's gate. **OPEN 3.** |

**Verdict changes: none after the timeout fix.** The first-run failure was a harness budget, recorded here as a finding (it would also turn `diff_core_recorded` red for 141FEE390 under load without the driver fix).

## 5. Why the layer grew by 577 members, and the one rule I changed to absorb it

Pulling a tail target into the universe also pulls its whole call tree (the existing out-of-set walk), and those functions then appear in the closure of the recorded functions that reach them. `14037CB50` (a huge function whose jump tables decode as a window of dead code) became reachable under the core set as well as the others, and two sets translate it differently (a callee linked in one, an out-of-set abort in the other): `gen_translated` raised a conflict because both variants had gaps. The rule is now: the variant with **fewer gaps** wins (`choose_variant`), a tie that differs is still a conflict. Consequence (INF, not run): a recorded function proven under the variant with more gaps now links the variant with fewer; paths that previously aborted now execute real code; the harness re-run above (13 functions) is the evidence it stayed bit-exact. `gen_translated` held out the same 5 recorded functions as before (a closure reaching a mistranslated callee: 1403BB670, 1403C55D0, 140554700, 1405549C0, 14202BA30).

## 6. Commits (all pushed; `ls-remote origin NativeSeam` = `970a8d47`)

`736ae6e8` merge origin/main; xlate loud + closure walk + layer check + red test; libm models; `1af2d0cd` layer regenerated (592 files); gen variant choice; driver compile budget; test docstring/path. Changed tools: `xlate.py`, `driver.py`, `gen_translated.py`, `stubs.py`, `stubs.json`, `diffharness/run.py`, `pes_stubs.hpp`, `tests/pilot/CMakeLists.txt` (+2 ctests), two new tests. No machine paths in the changed files (grep; `PES_UCRT_SNAP` has no default path).

## 7. Fast-tier line (interim form: `GATE_TIER` does not exist on this branch)

`GATE fast (interim): PARTIAL`. Run: `gen_translated.py --check` **GREEN** (4588 functions byte-identical); `test_tail_jump_loud.py` 9/9, `test_libm_models.py` 5/5 (with the snapshot), `test_native_services.py` 3/3; `g++ -fsyntax-only` on the changed audio/cutscene/replay files; 13 recorded functions re-diffed (§4). **Not run: the `pes_*` C++ binaries** (`pes_stubs.hpp` changed, so the whole 4,588-file layer recompiles: about an hour at -j3; the new `pes_model_*` are only referenced by 14231E894/964 and by builds that include them), the core recorded set, gfsim smoke (Main gates).

## 8. Open items

1. **`.impdata` code** (§2): five real tail calls into it are loud aborts; widen the code range and translate it, with ledger rows. The generation check will then cover them.
2. Name the other thunks of the `1415A17xx` page (`1415A17BC` first, double `floor`-like) the way logf/powf were: slot value -> ucrtbase code -> run in unicorn.
3. `1404386D0`'s diff needs its capture bank (Main's gate has it).
4. Exact ucrtbase logf/powf (0.35% / 0.15% one-ulp gap): replicate its algorithm; needs the ucrtbase tables from the user's module.
5. `diff_core_recorded` and the pilot build are slower now (closures of 350-600 TUs per function); the per-TU parallel compile is the upgrade.
