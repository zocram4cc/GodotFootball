# pes-port-CutsceneFix: cutscene failure classes, fixed at the cause (25-10-26)

Branch `CutsceneFix`, head `e7d3414` (ls-remote confirmed), merged with origin/main (merge `5ccf03a`, includes batch AA). Commits: `aa6fe82` (WIP translator and generator), `c9b2332` (dispatch restored; pilot green), `e7d3414` (final translator, regenerated cutscene specs, full-run record). Worktree `<workdrive>/gf-CutsceneFix`, scratch `<workdrive>/gf-CutsceneFix-scratch`. Gate not run (Main gates). Image `pes_full.bin`; unicorn and capstone from the DiffHarnessRE venv.

## Result

| set | committed before | after (this branch) | lost | gained |
|---|---|---|---|---|
| cutscene, all 833 | 509 bit-exact | **549** bit-exact | 0 | 40 |
| cutscene seeds (751) | 447 (59.5%) | **483 (64.3%)** | 0 | 36 |
| pilot, `--check` (183) | 183 | 183 | 0 | 0 |
| impure, `--check` (8) | 8 | 8 | 0 | 0 |

Not reached: the 70% bar (526 of 751). 43 more seeds are needed; the open classes are below.

Failure classes, all 833 (before to after): translated 830 to 833; struct map gap 209 to 210; translator bug 60 to 16; harness cannot run 55 to 58.

Seeds not bit-exact: map gap 200 to 201, translator bug 56 to 16, harness cannot run 48 to 51. Median own-block coverage of seeds: 0.6 before and after. The +1 map gap and +3 harness-cannot-run are functions that now translate (the three TypeErrors) or that the generator fixes moved from a spec error to a map fault (0x1414BB950, 0x141F79FF0).

## What was fixed (at the cause)

Translator (`tools/dev/translate/xlate.py`, `pes_state.hpp`):
- **Silent UNHANDLED no-ops** (the `/*UNHANDLED*/(void)0` fall-through hid these): MOVABS as MOV (imm64 loads were dropped; 16 functions); LOCK prefix dropped (single-threaded harness); ROR/ROL with CF/OF; XADD; CMPXCHG; PXOR/PAND/POR/PANDN; MOVDQA/MOVDQU; packed PS (ADDPS/SUBPS/MULPS/DIVPS); packed dword (PADDD/PSUBD/PCMPEQD/PSLLD/PSRLD/PSRLDQ); shuffles (PSHUFD/SHUFPS/UNPCKLPS/UNPCKHPS/PUNPCKLDQ/PUNPCKHDQ); CVTTPS2DQ; scalar double (ADDSD/SUBSD/MULSD/DIVSD, COMISD/UCOMISD via `cmp_sd`, CVTTSD2SI, CVTSI2SD).
- **ANDNPS** always produced 0 (accumulator started at zero): fixed.
- **TypeError** x3 (0x140005530, 0x1400DB970, 0x140104DA0): `parse_mem` rejected the `riz`/`eiz` zero-index pseudo-register of alignment NOPs. Fixed; all three now translate.
- **Regression caught and fixed**: my first batch used `PUT N:` (replace) where I meant insert, and clobbered two dispatch lines (MUL/BT/CDQ/CQO and CVTPS2PD/CVTPD2PS/CVTDQ2PS). Restored in `c9b2332`. The pilot check showed 31 regressions before the fix and 0 after.
- **One seed lost by partial SSE** (0x14002AB20): the function uses PSRLD/PSLLD/PUNPCKLDQ/CVTTPS2DQ, which were silent no-ops. With only the packed-float ops implemented, its xmm0 was wrong; the old pass was a coincidence. Implementing the four missing ops made it bit-exact (`e7d3414`).

Generator (`tools/dev/diffharness/specgen/spec.py`), validated by pilot regeneration (252/252 identical):
- **Negative field offsets** wrote `"0x-1"` in input/output offsets and crashed the harness (`ValueError` at 0x141F79FF0). The error is in the input offset, not in `objname` as SpecGen's note guessed. Offsets are now signed text (`off_hex`); a zero-filled pad region is placed below any object with a negative field, so those accesses land in mapped memory.
- **Region overlap** (0x1414BB950): the fixed 4 MB stride overlapped any region larger than 4 MB. Bases are now cumulative (`base += max(stride, size)`); identical to before for every spec whose regions fit the stride.
- `objname` parses negative path elements with `parse_off`, not `int(p, 16)`.
- 10 cutscene specs changed (region bases only, plus 0x1414BB950 and 0x141F79FF0); all regenerated and committed.

## Map faults: classified, with evidence (209 before, 210 after)

Method: a scratch wrapper around `unicorn.Uc.emu_start` records the faulting instruction, its base and index registers, and the effective address (`<workdrive>/gf-CutsceneFix-scratch/diag_run.py`, `diag_orch.py`). The fault is an exception raised in `emu_start` (UC_ERR_MAP), not the "unmodelled" hook. All 202 are loads through a register pointer.

Classes by effective address (209 runs):
- **small address, null base register: 108**: a pointer read from the object is 0.
- **small address, non-null base: 52**.
- **past the end of arg root A0 (within 64 KB): 17**.
- **other address: 20**.
- **write fault: 4** (0x1402A51C0, 0x1402A51E0, 0x1403FB710, 0x1403FB730; `UcError: Invalid memory write` raised from `run_case`, outside `emu_start`).
- **arg root A0: 3**; **past end of arg child: 2**; **no EA: 1** (0x14175850F, a CPU exception); **no diagnostic: 2**.

Sources of the null base (trace of the defining instruction of the base register; 150+ cases):
1. **Pointer arithmetic on scalar arguments, about 50+** (`sub rcx, r8`, `add`, `imul`, `lea`). Example 0x1404705B0 (and 0x14047DD60): a memmove-like copy computes `rcx = src - dst`, then loads `[rcx + r8]`. The spec models `rcx`/`rdx` as u8 scalars, so src is near 0. Needs pointer-origin inference through `sub`/`add` in `analyze.py`: not started.
2. **Pointer tables indexed by an input, about 37** (`[r9 + rcx*8]`, `[rbx + rcx*8]`, `[rcx + r8*8 + 0xf8]`). The table slots are zero and not linked. Needs element links for a region table (the G registry blanket is the only existing case).
3. **Stack argument used as a pointer, about 12** (`[rsp + 0x28]`). The generator models the fifth argument as a u8 scalar.
4. Other (a field whose object is not in the spec, or a value defined by `cmp`): the remainder.

**Hypothesis tested and rejected as the driver:** the blanket E1EE slot expansion on G (pilot mode) was replaced by impure mode (accessor-linked G, no blanket) on the same 833 specs. Result: map faults 209 to 205 (-4); 3 bit-exact functions lost (0x14048AFE0 and 0x140A62690, both G-slot reads the accessor layout does not link; 0x141F31290, an out-of-set callee classified unmodellable) against 14 gained. Impure mode is therefore not used: the no-loss rule forbids it. The blanket is needed for those two functions, so the accessor layout is incomplete for G.

Not fixed in this session (open; each is a generator or analysis change, not a translator change): classes 1 to 3 above.

## Other open items

- 0x14175850F: UC_ERR_EXCEPTION at RIP 0x14175850F, not diagnosed.
- 0x1402A51C0 family: 4 write faults, raised in `run_case` outside `emu_start` (patch or input writes), not diagnosed.
- Remaining translator bugs (16): 0x140227E70 (`ret_rax`), 0x140286A40 (timeout), 0x1408D10B0, 0x1408D1410 (timeouts), 0x1408D4A50 (`sub` overflow chain), 0x1408D4E40 (imul high half), 0x140A3F770, 0x141AE8950, 0x141E9FAE0 (alias of the output region through r9/rcx), 0x141F32300 (`ret_eax` from a tail path), 0x141F4C880, 0x141F4C8C0 (timeouts), 0x141F56FD0 and 0x141F57910 (compile: a label the translator did not emit), 0x141F5B810 (timeout), 0x141F61660 (`imul rax, r8`), 0x14202C250 (timeout). The timeouts are loops the translator does not terminate on; not diagnosed beyond that.
- Shift CF/OF flags (`do_shift` sets `cf = of = false`) are not corrected: not in any mismatch.
- ANDNPS: the `cur` fix is untested, because no corpus function hits ANDNPS.

## Verification

- Pilot `--check` (183 recorded-diffed): 183/183 bit-exact on the final translator (`e7d3414`).
- Impure `--check` (8): 8/8 bit-exact on the final translator.
- Cutscene full run on final code: 833 attempted, 833 translated, 549 bit-exact; that run's record is `tools/dev/translate/results_cutscene.json`, committed in `e7d3414`.
- Cutscene `--check` on the committed record: **549/549 bit-exact** (passed).
- Gate not run. Main should run the gate's `diff_cutscene_recorded` (549), `diff_pilot_recorded` (183), and the impure check (8).

## Process notes

- About 150 tool calls. Full runs: impure cutscene (71 min, old translator), cutscene (65 min, twice), cutscene `--check` (9 min), pilot checks (3).
- The branch was pushed as WIP (`aa6fe82`) before the first green check, which is against the brief; the record was not changed until the full run on the final code.
