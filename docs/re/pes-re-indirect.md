# Indirect calls and switch tables: static resolution (IndirectResolve, 25-10-26)

Compiled by Main from IndirectResolve's final report (it ran out of budget before writing this) and Main's full run
of the tool. Static only (PES not running); image `pes_full.bin`, whose `.data` holds dump-time values, so a pointer
read there is corroborating evidence, never proof. Tool: `tools/dev/ledger/indirect.py` (branch `origin/Indirect`
`4cf22c7`), deterministic, image only, about 30 s over the whole code section.

## Result

| In-scope indirect sites (6,873, Main's full run) | Count |
|---|---|
| Vtable dispatch, candidate set only | 6,718 (98%) |
| Plain register calls, unresolved | 155 |

Switch tables resolve completely: the guard's `cmp/ja` proves the bound and every case is read from the image (three
table shapes: base+rva, base+map+rva, base+cdqe+map+rva). Function-pointer tables in `.data` mostly resolve to code
(dump value, uncorroborated). Imports are named from thunk targets and IAT slots.

**Vtable calls do not resolve statically.** Each site resolves only to the in-scope vtables that have that slot
(thousands of candidates). The constructor join (`LEA reg,[vtable]; MOV [obj],reg`) gives a single target only where
the receiver is a fixed global, and the two such sites in the sample (`0x1404F8753`, `0x1404FC005`) read a global that
was unset at dump time.

## What it means for the pipeline

Resolving targets ahead of time is the wrong goal for 98% of the sites. The approach used by whole-program static
recompilers is: translate every function, and emit each indirect call as a lookup of its runtime target address in a
table mapping PES addresses to translated functions. PES's objects carry their own vtable pointers, so dispatch picks
the right function at run time exactly as PES does. The remaining work is that table, and specs that build real objects
(with their vtable pointers) as inputs.

## Impure sample (85 unmodellable callees, ImpureMap's list)

- 63 indirect call sites: 51 vtable two-step (`mov reg,[obj]; call [reg+off]`, slots +0x8 ×15, +0x10 ×17, +0x88 ×11,
  +0x18 ×3, +0x20, +0x30, +0x38 ×3), 6 import/RIP, 2 stack-object vtables, 1 register call, 1 unknown, 4 stack callbacks.
- 23 switch sites (`jmp reg` after a relative table load); all enumerable.
- 4 IAT slots named: `0x14252FDE0` abort trap, `0x14252FD98` engine constructor, `0x14252F6E8` CRT init,
  `0x142530868` CRT TLS callback. 1 logf/powf thunk, 1 global store.

## Spec fixes (impure specs, scratch)

- `0x14053B420`: accessor `0x140A489F0` (base 0xF3, n 0x16) was missing, leaving its record slots unlinked.
- `0x14065C2A0`, `0x140978E70`: `G+0x180` is a table base (stride 0x74), modelled as a scalar; relinked as a table.
- `0x14097CD00`: `regions_init` named an IMG region the spec never declared; dangling entries dropped.
