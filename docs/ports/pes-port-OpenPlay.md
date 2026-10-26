# OpenPlay: argument-root typing from caller bytes, and the union with the supported typings (26-10-26)

Branch `OpenPlay` (worktree `<workdrive>/gf-OpenPlay`, base `origin/Captures` f5a42e32). Pushed: `57d4f1aa` (typer, typed roots, evidence) and `4dd7257c` (full-run records, 89-root table). `ls-remote` confirmed both. Gate not run (agents do not run the gate). No new captures were taken. This covers item 1 of the reassigned scope: type the argument roots, re-run the union, commit, report. The frame-loop trace belongs to FrameTrace.

## 1. What was decoded or changed

- `tools/dev/translate/argroots.py` rewritten. Each direct call or jump (E8/E9) into an impure spec is one site. The containing function comes from the image's x64 unwind table (.pdata: 158,619 functions after dropping 58 corrupt entries over 1 MB; all 50 impure specs are exact function begins, CONFIRMED by `pdata_check.py`). Each function is walked linearly from its entry to the site, tracking what each register holds: the entry argument `("arg", r)`, an accessor result or G-table entry `("rec", class)`, the G object, or unknown. Blocks entered by jumps join the states recorded at those jumps. Stack spills are followed. An argument is resolved through its function's callers (depth 4) or through the function's typed root.
- The previous typer took the enclosing function as the nearest spec VA below the site and used a 40-instruction window. Both are wrong for these sites (section 2).
- Result: 97 argument root slots over the 50 specs (A0..A3 in rcx/rdx/r8/r9, per each spec's `args`). **8 typed, 89 untyped.** All 8 are bindable (team or man). 42 specs now have no typed root (the previous count of 35 is 15 typed; 7 of those are withdrawn).

## 2. Typed roots (`impure_roots.json`, `impure_roots_evidence.json`)

| root | class | basis |
|---|---|---|
| 1404F47A0.A0 | team | RealCases seed (INF, not re-derived here) |
| 1404F6C30.A0 | team | RealCases seed (INF) |
| 1404F8C70.A0 | team | RealCases seed (INF) |
| 1404FA720.A0 | team | RealCases seed (INF) |
| 14064DE60.A0 | man | RealCases seed (INF) |
| 140978E70.A0 | man | RealCases seed (INF) |
| 1404FE9C0.A0 | team | CONFIRMED bytes: `mov rcx, rdi` then `call 1404FE9C0` at 1404F6D60 inside 1404F6C30, before rdi is overwritten at 1404F751A. Conditional on the seed 1404F6C30.A0 (INF) |
| 1404FEDC0.A0 | team | CONFIRMED bytes: call at 1404F6DA5 inside 1404F6C30, same condition |

Withdrawn from the 15 previous typings (7). Each is unsupported at its site under exact function bounds and overwrite-aware origins:

- 1404FB2A0.A0 at 1404F7846 and 14057BA50.A0 at 1404F7998: rcx = rdi, but rdi is reloaded from `[rsp+0x40]` at 1404F751A, before both sites.
- 140503430.A1 at 140502B57: the .pdata containing function is 140502A3D..140503407, not 1404FEDC0; rdi there has no known origin.
- 1406486D0.A0 at 14064E43F: the containing .pdata entry is 14064E3F9..14064E464 (a 0x6B-byte block, not 14064DE60); rsi has no known origin.
- 14064DC50.A0 at 14065426D (rsi, unknown origin) and 140654E60.A3 at 1406507F9 (r9 = rsi, unknown origin): both in functions whose argument does not resolve through callers.
- 14065C2A0.A0 at 14065283E: the argument resolves to the containing function's rcx (140651430..140653478, not 14064DE60), whose callers give no class.

## 3. Union measurement (17 captures; 8 roots bound, 42 specs on G-only seeds)

Captures: 13 from `captures_i` (t001..t014 minus period-end t003) plus 4 from `captures_f` (t034, t037, t041, t046). Bank order fixed by sorting the capture paths. Full runs: `drv_A` (15 roots, the f5a42e32 set) and `drv_B` (8 roots). Committed as `results_impure_15roots_sorted.json` and `results_impure_8roots_sorted.json`.

| run | floor (bit-exact and own ≥ 0.50) | bit-exact | median own | mean own | specs with coverage |
|---|---|---|---|---|---|
| A: 15 roots (f5a42e32 set), sorted order | 16 | 16 | 0.299 | 0.357 | 46 |
| B: 8 roots (57d4f1aa), sorted order | 16 | 16 | 0.299 | 0.357 | 46 |
| Captures' record, 15 roots, its own order | 16 | 16 | 0.299 | 0.357 | 46 |
| Captures' re-measure, 8 roots, its own order (origin/Captures 0b0919e1, reported by Captures) | 16 | 16 | not re-read here | not re-read here | — |
| my first 8-root run, unsorted order (`drv_v5`) | 13 | 13 | 0.255 | 0.334 | 46 |

Reading:
- Typing 15 → 8 changes no floor membership and no median. The 7 withdrawn typings did not move any spec across the floor. Differences between runs A and B: 0x1404FB2A0 cases 28->25 own 0.75->0.75 diffed True->True; 0x140503430 cases 128->128 own 0.6102->0.5932 diffed True->True
- The floor is sensitive to capture order at the bank level: the same 8 roots give floor 16 in sorted order and 13 in unsorted order (three specs flip: 140529490, 14053B110, 1405AD720; all untyped in both, so the flip is order, not typing). The bank binds instance `n % N` for case `n`, so the order of the `--live` directories changes every case's instance. The fix is to sort the capture directories in `seed.py` `bank_multi`. I did not change `seed.py` (Captures' file). Any union number must state its order.
- Against Main's figures: the union floor is 16 (median 0.299) for both typings, and the lift over the single snapshot (13, median 0.288) comes from the captures, not from the typing. Per-situation (stoppage vs open play) coverage was not measured in this item.

## 4. The 89 untyped roots (for checking FrameTrace's traces)

Totals over unknown-origin sites: copy of an unknown register 176, field load 55, `lea` 40, stack reload 21, computed 2 (294 sites). The 14 rows whose shape is "no writer in fn" in the data below are roots whose every site takes the argument of a function whose callers do not resolve statically (dispatch or indirect callers). The same table is committed as `tools/dev/translate/untyped_roots.tsv`.

What blocks each shape:
- **Copy of an unknown register** (most sites): the argument register holds a copy of a register whose own value came from a load or call the walk cannot follow. A run-time trace reads the real value.
- **Field load** `[reg+disp]`: a pointer read from a field of a record of unknown class. The docs document only the team child slots `+0x6D48`/`+0x6D50` as pointers, and the captures show them null (RealCases F3/F5), so no class can be bound from the captures.
- **Stack reload**: an argument spilled then reloaded from a slot whose stored value has no known origin.
- **`lea`**: the argument is a sub-object or a stack address, not a record of a bindable class.

From argmap (the field-offset base): the 35 specs of the previous untyped set are 25 ambiguous small-offset cases, 7 with no A0 offsets, 3 with A0 not in rcx. None is decisive, so the field offsets type none of them.

## 5. Open items and next steps

1. The 89 roots need run-time evidence: FrameTrace's traced real calls give the argument's record, so the class of each root can be read from the live call (bind by the same root names).
2. The roots whose argument comes from a function with unresolved callers need the runtime caller, the same trace.
3. Capture order: make `bank_multi` sort its live directories (Captures' `seed.py`), so union floors are reproducible without a stated order.
4. Per-situation coverage (stoppage vs open play, per function) was not measured. It needs one driver run per situation bank.
5. Tests: none for the typer. Verified by the regression against the committed 15 (7 withdrawn, with per-site evidence above) and by the two full driver runs (rc 0 both; 46 translated, 16 bit-exact each).

## 6. The 89 untyped roots

| spec | arg | reg | unknown sites | shapes | first site | containing fn | last writer at first site |
|---|---|---|---|---|---|---|---|
| 1404386D0 | A0 | rcx | 1 | {lea: 1} | 140440C2A | 14044090C | `lea rcx, [rsp + 0x28]` |
| 1404386D0 | A1 | rdx | 1 | {lea: 1} | 140440C2A | 14044090C | `lea rdx, [rsp + 0x98]` |
| 1404405D0 | A0 | rcx | 1 | {field load: 1} | 14041EE57 | 14041ED22 | `mov rcx, qword ptr [r14 + 0x460]` |
| 1404405D0 | A2 | r8 | 1 | {lea: 1} | 14041EE57 | 14041ED22 | `lea r8, [rsp + 0x20]` |
| 1404F6C30 | A2 | r8 | 1 | {field load: 1} | 14041EDD7 | 14041ED22 | `mov r8, qword ptr [rbx]` |
| 1404FB2A0 | A0 | rcx | 1 | {copy of register: 1} | 1404F7846 | 1404F6C30 | `mov rcx, rdi` |
| 140503430 | A0 | rcx | 1 | {copy of register: 1} | 140502B57 | 140502A3D | `mov rcx, r13` |
| 140503430 | A1 | rdx | 1 | {copy of register: 1} | 140502B57 | 140502A3D | `mov rdx, rdi` |
| 140503430 | A2 | r8 | 1 | {lea: 1} | 140502B57 | 140502A3D | `lea r8, [rsp + 0x50]` |
| 140503430 | A3 | r9 | 1 | {lea: 1} | 140502B57 | 140502A3D | `lea r9, [rsp + 0x68]` |
| 140504D20 | A0 | rcx | 1 | {copy of register: 1} | 140506AB9 | 140506A87 | `mov rcx, rbx` |
| 140504D20 | A1 | rdx | 1 | {copy of register: 1} | 140506AB9 | 140506A87 | `mov rdx, rdi` |
| 1405055E0 | A1 | rdx | 1 | {copy of register: 1} | 140506B0F | 140506A87 | `mov rdx, rdi` |
| 1405055E0 | A2 | r8 | 1 | {lea: 1} | 140506B0F | 140506A87 | `lea r8, [rsp + 0x20]` |
| 140509A40 | A0 | rcx | 3 | {copy of register: 2, stack reload: 1} | 1405EC2C0 | 1405EC21A | `mov rcx, r15` |
| 140510D30 | A0 | rcx | 1 | {no site: 1} | 0 | 0 | `` |
| 140510D30 | A1 | rdx | 2 | {copy of register: 1, field load: 1} | 140505952 | 14050590A | `mov rdx, r15` |
| 14051B8B0 | A0 | rcx | 2 | {copy of register: 2} | 14051DD16 | 14051DD10 | `mov rcx, rbx` |
| 14051B8B0 | A1 | rdx | 2 | {copy of register: 2} | 14051DD16 | 14051DD10 | `mov rdx, r14` |
| 14051CF70 | A0 | rcx | 2 | {copy of register: 2} | 14051DACC | 14051DA13 | `mov rcx, rbx` |
| 14051CF70 | A2 | r8 | 2 | {copy of register: 2} | 14051DACC | 14051DA13 | `mov r8, r13` |
| 140529490 | A0 | rcx | 3 | {copy of register: 1, stack reload: 2} | 140532BBA | 140532841 | `mov rcx, qword ptr [rsp + 0x40]` |
| 14052A190 | A0 | rcx | 2 | {copy of register: 2} | 14052FA9F | 14052F9AE | `mov rcx, rbx` |
| 14052A820 | A0 | rcx | 7 | {copy of register: 7} | 1405270AA | 140526A14 | `mov rcx, r13` |
| 14052A820 | A1 | rdx | 7 | {copy of register: 4, stack reload: 3} | 1405270AA | 140526A14 | `mov rdx, qword ptr [rsp + 0x138]` |
| 14053B110 | A0 | rcx | 1 | {lea: 1} | 1404F783E | 1404F6C30 | `lea rcx, [rdi + 0x6ba8]` |
| 14053B110 | A3 | r9 | 1 | {field load: 1} | 1404F783E | 1404F6C30 | `mov r9, qword ptr [rdi + 0x6d50]` |
| 14053B420 | A0 | rcx | 1 | {no site: 1} | 0 | 0 | `` |
| 14053B420 | A1 | rdx | 1 | {no site: 1} | 0 | 0 | `` |
| 14057A3E0 | A2 | r8 | 3 | {copy of register: 3} | 140579AFE | 1405799A3 | `mov r8, rbp` |
| 14057BA50 | A0 | rcx | 1 | {copy of register: 1} | 1404F7998 | 1404F6C30 | `mov rcx, rdi` |
| 14057D3A0 | A0 | rcx | 1 | {no site: 1} | 0 | 0 | `` |
| 1405A6DE0 | A0 | rcx | 2 | {copy of register: 2} | 14059CC01 | 14059BCBC | `mov rcx, r12` |
| 1405A6DE0 | A2 | r8 | 2 | {lea: 2} | 14059CC01 | 14059BCBC | `lea r8, [rsp + 0x50]` |
| 1405AD720 | A0 | rcx | 1 | {copy of register: 1} | 14050500B | 140504D20 | `mov rcx, r13` |
| 1405AD720 | A1 | rdx | 1 | {copy of register: 1} | 14050500B | 140504D20 | `mov rdx, rbx` |
| 1405CCD50 | A0 | rcx | 1 | {copy of register: 1} | 1405CE3CC | 1405CE381 | `mov rcx, r13` |
| 1405CCD50 | A1 | rdx | 1 | {copy of register: 1} | 1405CE3CC | 1405CE381 | `mov rdx, rdi` |
| 1405CE4F0 | A0 | rcx | 1 | {lea: 1} | 140682065 | 140681FF0 | `lea rcx, [rbx + 0x50]` |
| 1405CE4F0 | A1 | rdx | 1 | {lea: 1} | 140682065 | 140681FF0 | `lea rdx, [rsp + 0x40]` |
| 1405CE930 | A0 | rcx | 1 | {no site: 1} | 0 | 0 | `` |
| 1405CE930 | A1 | rdx | 1 | {no site: 1} | 0 | 0 | `` |
| 1405DC350 | A0 | rcx | 4 | {copy of register: 4} | 1405DB3D9 | 1405DB356 | `mov rcx, rdi` |
| 1405DC350 | A1 | rdx | 4 | {copy of register: 1, field load: 3} | 1405DB3D9 | 1405DB356 | `mov rdx, qword ptr [rbp + 0x5f]` |
| 1405DC350 | A2 | r8 | 4 | {copy of register: 4} | 1405DB3D9 | 1405DB356 | `mov r8, r15` |
| 1405DC350 | A3 | r9 | 4 | {copy of register: 4} | 1405DB3D9 | 1405DB356 | `mov r9, rbx` |
| 1405DE950 | A1 | rdx | 3 | {lea: 3} | 1405D07F4 | 1405D0700 | `lea rdx, [rbp - 0x49]` |
| 1405DE950 | A2 | r8 | 3 | {copy of register: 1, field load: 2} | 1405D07F4 | 1405D0700 | `mov r8, qword ptr [rbp + 0x4f]` |
| 1405DE950 | A3 | r9 | 1 | {copy of register: 1} | 1405DCA94 | 1405DC4D6 | `mov r9, r12` |
| 1405E4460 | A0 | rcx | 1 | {copy of register: 1} | 1405DB2FF | 1405DB18E | `mov rcx, rdi` |
| 1405E4460 | A1 | rdx | 1 | {field load: 1} | 1405DB2FF | 1405DB18E | `mov rdx, qword ptr [rbp + 0x5f]` |
| 1405E4460 | A2 | r8 | 1 | {copy of register: 1} | 1405DB2FF | 1405DB18E | `mov r8, r15` |
| 1405E4460 | A3 | r9 | 1 | {copy of register: 1} | 1405DB2FF | 1405DB18E | `mov r9, rbx` |
| 140601110 | A0 | rcx | 1 | {no site: 1} | 0 | 0 | `` |
| 14062E240 | A0 | rcx | 1 | {no site: 1} | 0 | 0 | `` |
| 14062E240 | A1 | rdx | 1 | {no site: 1} | 0 | 0 | `` |
| 14062E240 | A2 | r8 | 1 | {no site: 1} | 0 | 0 | `` |
| 140639100 | A0 | rcx | 4 | {lea: 4} | 140697644 | 1406975A2 | `lea rcx, [rbp - 0x59]` |
| 140639100 | A1 | rdx | 4 | {copy of register: 1, field load: 3} | 140697644 | 1406975A2 | `mov rdx, r15` |
| 140639100 | A2 | r8 | 4 | {lea: 4} | 140697644 | 1406975A2 | `lea r8, [rbp + 0x67]` |
| 1406486D0 | A0 | rcx | 2 | {copy of register: 2} | 14064E43F | 14064E3F9 | `mov rcx, rsi` |
| 1406486D0 | A1 | rdx | 2 | {copy of register: 2} | 14064E43F | 14064E3F9 | `mov rdx, r13` |
| 14064DC50 | A0 | rcx | 2 | {copy of register: 2} | 14065426D | 140653FC0 | `mov rcx, rsi` |
| 14064DE60 | A1 | rdx | 2 | {lea: 2} | 1405C934E | 1405C81ED | `lea rdx, [rbp + 0x120]` |
| 140654E60 | A2 | r8 | 7 | {copy of register: 5, lea: 2} | 14064C888 | 14064B630 | `mov r8, r13` |
| 140654E60 | A3 | r9 | 9 | {copy of register: 5, lea: 4} | 14064C888 | 14064B630 | `mov r9, rsi` |
| 14065C2A0 | A2 | r8 | 1 | {stack reload: 1} | 1406520C5 | 140651430 | `mov r8, qword ptr [rsp + 0x70]` |
| 14065C2A0 | A0 | rcx | 1 | {copy of register: 1} | 1406520C5 | 140651430 | `mov rcx, r12` |
| 14065C2A0 | A1 | rdx | 2 | {copy of register: 2} | 1406520C5 | 140651430 | `mov rdx, r14` |
| 14065DAB0 | A0 | rcx | 2 | {copy of register: 1, stack reload: 1} | 14064FDFB | 14064FDCA | `mov rcx, rbx` |
| 140687310 | A0 | rcx | 2 | {copy of register: 2} | 140686A4B | 140686190 | `mov rcx, r11` |
| 140687310 | A1 | rdx | 2 | {copy of register: 2} | 140686A4B | 140686190 | `mov rdx, r10` |
| 140687310 | A3 | r9 | 2 | {copy of register: 1, stack reload: 1} | 140686A4B | 140686190 | `mov r9, qword ptr [rsp + 0x78]` |
| 14080EBA0 | A0 | rcx | 7 | {lea: 7} | 14081512B | 140814640 | `lea rcx, [rsp + 0x40]` |
| 14080EBA0 | A1 | rdx | 7 | {computed: 2, copy of register: 5} | 14081512B | 140814640 | `mov rdx, rax` |
| 140975710 | A0 | rcx | 1 | {copy of register: 1} | 14097665B | 140976522 | `mov rcx, r13` |
| 140975710 | A1 | rdx | 1 | {copy of register: 1} | 14097665B | 140976522 | `mov rdx, r15` |
| 140975710 | A2 | r8 | 1 | {lea: 1} | 14097665B | 140976522 | `lea r8, [rsp + 0x64]` |
| 140975710 | A3 | r9 | 1 | {lea: 1} | 14097665B | 140976522 | `lea r9, [rsp + 0x70]` |
| 140978E70 | A2 | r8 | 1 | {no site: 1} | 0 | 0 | `` |
| 140978E70 | A1 | rdx | 1 | {no site: 1} | 0 | 0 | `` |
| 14097CD00 | A0 | rcx | 1 | {copy of register: 1} | 14097B798 | 14097B77B | `mov rcx, rdi` |
| 14097FCE0 | A0 | rcx | 1 | {no site: 1} | 0 | 0 | `` |
| 14097FCE0 | A2 | r8 | 1 | {lea: 1} | 14097F2D3 | 14097E310 | `lea r8, [rsp + 0x58]` |
| 14097FCE0 | A3 | r9 | 1 | {no site: 1} | 0 | 0 | `` |
| 14097FFF0 | A0 | rcx | 1 | {copy of register: 1} | 14097C3BF | 14097C240 | `mov rcx, rdi` |
| 14097FFF0 | A2 | r8 | 1 | {copy of register: 1} | 14097C3BF | 14097C240 | `mov r8, rsi` |
| 140A539F0 | A0 | rcx | 3 | {copy of register: 1, field load: 1, stack reload: 1} | 1404FC16F | 1404FC10F | `mov rcx, rsi` |
| 140A57650 | A0 | rcx | 132 | {copy of register: 80, field load: 41, stack reload: 11} | 140506F39 | 140506E5A | `mov rcx, r12` |


## Main's note

The floor count depends on capture order (16 sorted vs 13 unsorted for the same roots), because `seed.py` binds instance `n % N`. Until binding is order-independent (e.g. every case drawn over every instance), floor counts from different runs are only comparable at the same order.
