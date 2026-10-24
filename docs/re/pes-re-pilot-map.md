# PES AI pilot set: struct field map and harness specs (PilotMap-2 coverage report)

Scope: the 252-function pilot set (`local://pilot-ai-set.json`, main 19a19c7). Image: unpacked PES 2021, file offset = VA − 0x140000000. Scratch: `<workdrive>/gf-Pilot-scratch/map/`. Nothing committed; no PES bytes, constant values or decompiled text in this report.

## 1. Deliverables (all in scratch, handed to PilotTranslator-2)

| file | content |
|---|---|
| `specs/pilot/<VA>.json` (252) | diff-harness spec per function (run.py format; 64 cases, 2 MB regions, `precheck`, `ret_note`) |
| `fieldmap.json` | record types (one entry each), per-field `off/width/kind/rw/gf/cite`, per-function `bindings` |
| `results.json` | per function: `harness` (runnable / harness cannot run), `failure_class`, `note`; `translated`/`diffed` left false for the translator |
| `batch1.txt` | 50 no-callee leaves, smallest first (46 runnable, 4 classed) |
| `analysis.json`, `validation.json`, `diagnosis.json`, `manifest.json` | intermediate outputs (scripts: `analyze.py`, `gen_specs.py`, `validate.py`, `diagnose.py`, `build.py`) |

## 2. Harness classes (252 functions)

| class | count | meaning |
|---|---|---|
| runnable | 145 | spec runs 3 cases clean on the unicorn harness (no faults, no live `.data` reads) |
| harness cannot run: live state | 86 | reads or writes a writable `.data` global at its on-disk value (the runtime value is unverified). The spec exists and runs, but the diff is provisional. |
| harness cannot run: struct map gap | 21 | fault in case 0–2: a pointer the function dereferences is zero in the harness (origin not linked). See below. |
| engine call / GPU / translator bug / harness-only | 0 | no indirect CALL in the set; every CALL resolves inside the set (translator's census agrees) |

The 21 struct-map-gap VAs: 0x140439F90, 0x140529330, 0x1405CCD00, 0x1405CD0F0, 0x1405CD320, 0x1405CFC80, 0x1405DFC70, 0x1405DFE40, 0x14062C400, 0x14063C9F0, 0x1409FBCE0, 0x140A3E5D0, 0x140A3E7D0, 0x140A4C930, 0x140A4C9A0, 0x140A4EAF0, 0x140A4EDF0, 0x140A533A0, 0x140A53DA0, 0x140A61F40, 0x141E5D4C0. Each fault is an unmodelled read at a small address (base 0 + displacement) or a read just past a region. Cause: a pointer returned by a callee or stored in an unmodelled global. The next step for these is the field map's `bindings`, which need the object behind that pointer.

## 3. Method (what is CONFIRMED, what is INF)

- **CONFIRMED (script-read from the image):** `analyze.py` decodes each function with capstone (pilot `size` = instruction count, confirmed by last-instruction check) and tracks register origins from the entry args (rcx/rdx/r8/r9) through pointer chains, stack spills, forward-branch merges and in-set callees (return origins flow back into rax). It emits every memory access as (object, offset, width, read/write, kind). 2,500 field records across the set.
- **Specs:** every non-pointer field becomes a per-case input (generic domain, labelled in the schema), every written field an output. Pointer fields link child regions via `regions_init`; global pointer slots (`.data`) via `patch_addrs`. Stack args use `stack_offset = 0x20 + 8k` (verified against cone.json).
- **Harness limits worked around (no harness edit):** `encode` has no u16 and `decode_blob` has no u64, so 2-byte fields are byte pairs and 8-byte outputs are u32 pairs; 16-byte fields are two u64.
- **Array (IDX) fields:** only the base element is an input; the rest of the array is zero-filled (the harness does not randomize arrays).
- **Return value:** `ret_rax` (pointer), `ret_eax` (integer/bool; upper bits unchecked), or `ret_xmm0` (float or void), from the last rax write before each ret. Heuristic; the translator should key on the signature.

## 4. Fieldmap coverage

- Record types with a named base (evidence cited in `fieldmap.json`): **T** (team; A0.10 / A1.10 / *.108), **MI** (match info; A0.28 / A1.28 / *.0; MI+0x116C play state), **MA** (match array; A0.18 / *.F8.18; MA+0x2482C ball copy), **T.lane** (T+0xE0 ring block; +0x1900 count, +0x1904 head), **slot** (per-player record; P/pl offsets). Global pointer objects (`global:1436F3DC8…`, 66 functions) are named by VA and provisional.
- **Provisional:** A1, A2, A3 roots (no evidence for their base). A0 is a slot record in most functions but can be the team record in others, so the A0 binding is per function (`bindings`).
- **Field gf mapping:** 21 fields have a GF member name (a declaration line citing `ALIAS+0xOFF`). **373 non-pointer fields are gap** (`gf: null`), i.e. the struct map has no GF carrier. Pointer fields (chain links) are not counted.
- Gaps in the struct map are the translator's `struct map gap` failure class.

## 5. Open items / caveats

1. Record-type binding per function (especially A0 slot vs team, and the A1/A2/A3 roots) needs a per-function read or a decompile pass. Until then the `bindings` are provisional.
2. 86 `live state` functions read `.data` globals on disk. Their diffs need the live value or a decision on how to model the global (the harness cannot write image VAs except via patch_addrs).
3. 202 memory operands across the set are not resolved (pointer origin lost, mostly callee-returned pointers or indexed bases). The harness shows which ones fault; the rest run on zeroed memory.
4. `tailjump` flags on 27 functions (a jump out of the function body); all of them run clean on the harness, but the callee should be verified.
5. Value domains are generic (u8 range 0..255 for ints; −50..50 for f32), labelled `generic ... (no doc range)` in the schema. No doc-derived domains yet.
6. Batch 1 handed to PilotTranslator-2 (`batch1.txt`); translated/diffed statuses are theirs to set.
