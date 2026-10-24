# PES AI pilot set: struct field map and harness specs (PilotMap-2 coverage report, v3)

Scope: the 252-function pilot set (`local://pilot-ai-set.json`, main 19a19c7). Image: unpacked PES 2021 dump, file offset = VA − 0x140000000. Scratch: `<workdrive>/gf-Pilot-scratch/map/`. Nothing committed; no PES bytes, constant values or decompiled text in this report beyond VAs and script-read constants.

Important: "runnable" means the spec runs 3 cases without a harness fault. It is not a bit-exact diff result. The diff verdicts are PilotTranslator-2's and Main's.

## 1. Class counts (252 functions)

| class | v1 | v2 | v3 (now) |
|---|---|---|---|
| runnable (3 cases, no fault, no live global) | 145 | 142 | **232** |
| harness cannot run: live state | 86 | 86 | **4** |
| harness cannot run: struct map gap | 21 | 24 | **16** |

Batch 1 (`batch1.txt`, 50 leaves): 48 runnable, 2 live state (0x14149A470, 0x141E5D4C0).

## 2. What changed in v3, and why

Writer evidence: `scan_writers.py` scanned the whole code section (`.trace`, 10.5 M instructions) for RIP-relative writes to each `.data` global the set reads directly (`writers.json`). The v1/v2 "live state" class was too coarse: `build.py` marked a function live if it read any non-`.rdata` global, and the manifest had collapsed every global to the name `IMG`. Each global now has its own class with the evidence:

| global VA | class | evidence | functions |
|---|---|---|---|
| 0x1436F3DC8 (match context G) | MODELLED: pointer patched to a synthetic G object | 2 writers (0x140A48782, 0x140A488FE); on-disk value is a dump-time heap pointer | 66 read it; not live |
| 0x14351AE50 / AE54 (tick rate 54) | CONST | sole writer is setter 0x141E532F0 (0x141E53300, 0x141E532F8), which stores 54.0 from `.rdata` 0x14259D624; on-disk bytes 0x42580000 equal it | 22 + 8 |
| 0x1436F3A40 block, 0x1436F3D60/64/70 (default / carded-out record) | CONST | no direct writer; on-disk kind word 0x37 routes A3E190 to the D60 fallback (code-read) | ~50 |
| 0x14351AE30 / AE34 (install tag / special card) | CONST | no direct writer; AE34 on-disk 0 = live 0 (pes-re-missing-systems.md); AE30 live value not read | 2 |
| 0x1434FC9C0 (mode→table) | CONST | no direct writer; on-disk values used | 2 |
| 0x1437F4808 (constant registry, WESYS) | LIVE | runtime list object filled per session (writers 0x141E5C964, 0x141E5CA0C); on-disk value is a heap pointer | 2 read directly |
| 0x143704E38 (match mode record) | LIVE | runtime object written by match setup (writers 0x14149A39F, 0x14149A53D, inside batch-1 gap fn 0x14149A470) | 2 read directly |

Registry model (`gen_specs.py`): the getter 0x141E5CA20 (tail-jumped from 0x141E5D4C0) is `[[list]+8·id]` then `+8` dereferenced. The spec models list → array (+0) → shared record (slot id) → payload pointer (+8) → payload object. Payload fields are not analysed, so they are zero: the registry functions stay `live state` with that reason. The bound check calls 0x141E5DE80, which returns the documented slot count 234 (`mov eax,0xea`, code-read); it is stubbed.

Analyzer changes (`analyze.py`): (a) origins are candidate sets, so a merge keeps every pointer candidate; (b) joins drop null/scalar paths; (c) an element of a global pointer table is its own origin (sentinel path element `0xE1EE`), which the registry model uses. Rejected: binding every argument register handed to a callee (wrong: a register is bound only when dereferenced), and an index-register base fallback (it bound scalar indices as objects and hid faults).

Domains (task 3): only the lane ring fields have doc-cited ranges so far. Ring count +0x1900 is 0..8 and head +0x1904 is 0..7 (pes-re-record-layouts.md §1); 7 specs use them. All other scalar inputs keep the generic label (u8 0..255; f32 −50..50). Not done: slot index 0..21, side 0..1, class byte 0..0x5D, holder 0..21/0xFF (all documented, not yet applied), and MI+0x116C, which has two readings in the docs (play state in pesclock.hpp:96 vs half-change counter in pes-re-tactics-clock.md §CE78) and must not be given a domain until resolved.

## 3. The 16 struct-map gaps (fault mechanisms, traced by `trace_fault.py`)

1. **Null return from a slot-index getter** (0x140632BC0, 0x140653480; probable 0x1405CD0F0, 0x1405CD320, 0x1405CFC80). Callees 0x140A48910 / 0x140A48A10 return `*(G+(i+K)·0x28)`, which is null for an index outside 0..21. The generated index is out of domain. Fix: slot index domain 0..21 (pes-re-record-layouts.md §2). Not applied yet.
2. **Lane-ring reader with a scalar in RDX** (0x140529330, 0x14062C400, 0x140A533A0, 0x140A74EC0). Callee 0x140A3E150 dereferences RDX as the ring base (`[rdx+0x1900]`); the caller's pointer argument is not resolved, so a generated scalar is bound. Needs the caller's RDX origin.
3. **Unresolved internal pointer** (0x140479470, 0x1405CCD00, 0x1405CD050, 0x140682320, 0x140A4EDF0, 0x140A51D50, 0x140A53DA0). A register is overwritten by an untracked load or callee return, then used as a base. Needs per-site decode.

## 4. Caveats on the CONST class (not yet verified)

- The image is a memory dump. A `.data` byte is an initializer only if nothing writes it. "No direct writer" excludes RIP-relative writes only. The default-record block has 15 address-taken sites (LEA of 0x1436F3A40) that were not checked for writes through the pointer. The docs say its consumers are read-only (INF).
- AE30's live value was not read.
- The scanner matches RIP-relative addressing only; absolute and register-held addresses are not covered.

## 5. Files (scratch, handed to PilotTranslator-2)

`specs/pilot/<VA>.json` (252, regenerated), `fieldmap.json`, `results.json` (per-function class and note), `batch1.txt`, `writers.json`, `analysis.json`. Scripts: `analyze.py`, `scan_writers.py`, `gen_specs.py`, `validate.py`, `diagnose.py`, `trace_fault.py`, `build.py`. v1/v2 snapshots: `results_v1.json`, `results_v2a.json`, `results_v3pre.json`.

## 6. Open items

1. Apply the documented domains (slot 0..21, side 0..1, class byte 0..0x5D, holder 0..21/0xFF); resolve the MI+0x116C conflict first.
2. Gap mechanisms 1–3 above.
3. Payload fields of the registry (dt18 values, stock `Data/dt18_all.cpk` via the VFS-read rule) so the registry functions can leave `live state`.
4. Mode record (0x143704E38): model its +0xF0 value from the setup writer, or keep as live with reason (current).
