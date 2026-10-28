# PES port SanctionSwitch: sanction switch NOT cut over. Decode gaps closed, two doc claims corrected, writers still open.

Branch `FoulOffside`, worktree `gf-FoulOffside`. Pushed: `6ed1a32b..69466b20` (`git ls-remote origin FoulOffside` = `69466b20`), the merge of `origin/main` (`e4431e45`). Labels: CONFIRMED = read from disassembly, decompile, the spec file or a binary scan this pass; INF = inference (reason given); OPEN = blocker named. Scratch (PES-derived, not committed): `gf-SanctionSwitch-scratch/` (`dec_cardtable.txt`, `dec_callers.txt`, `dec_recorder.txt`, `callers.py`, `pdata_entry.py`).

## 0. Status per ticket item

| # | Item | Status |
|---|---|---|
| 1 | `git merge origin/main`, push | **Done.** Merge `69466b20`, pushed and confirmed by `ls-remote`. |
| 2 | Close decode gaps (0x14042D5E0, 0x140A81230, kind-4 +0x20, `dl` flow, bytes 0x140480910 writes into A1) | **Partly done.** 0x14042D5E0 and `dl` closed. 0x140A81230 key/return closed with one open input (§2). 0x140480910 writes into A1: **none** (§4, corrects the RE doc). Kind-4 +0x20 still OPEN. |
| 3 | Build A1 in PES layout, call translated 0x14047F170 from the referee, stubs for presentation | **NOT done.** No code written. See §6 for why it cannot be done faithfully yet. |
| 4 | `SanctionDecision { scene_id, card_level, restart_kind }`, scene id wired to a cutscene trigger | **NOT done.** |
| 5 | TDD red/green from the oracle, mutations, `pes_*` suite, seeds 7 and 3 `--pes-loop` 10 min | **NOT done.** GF behaviour is unchanged, so no before/after exists to measure. No tests added, no mutations, no sim runs. |
| 6 | Clean cutover (delete GF sanction hand code) | **NOT done.** `referee.cpp` unchanged. |

The FoulOffside tree is at `69466b20`: the merge of `origin/main` and nothing else. No GF source or test changed.

## 1. Corrections to `docs/re/pes-re-sanction-switch.md` and `docs/ports/pes-port-FoulOffside4.md`

1. **0x140480910 does not write A1's card table (+0x6C).** The doc's partition row says it does, and that the next foul's 0x140A81230 read depends on the write. Neither part holds (§4). The spec for 0x14047F170 (`tools/dev/diffharness/specs/stoppage/14047F170.json`) has 53 input keys: none at A1+0x6C, none for A1 directly (only A1+0x18 and A1+0xE0 pointers), and no E0 key at +0x750D0 or +0x59650. CONFIRMED.
2. **`dl` in 0x140A3A690 is not "the foul-kind dword".** The stop==5 path (`MI+0x116C == 2`, `MI+0x1574 == 5`) passes `dl = (MI+0x1588 == 1)`, i.e. "foul kind is offside". The in-play path (`MI+0x116C == 1`) passes `dl = (MI+0x1544 == 1)`. The FoulOffside4 §1 claim "dl = (MI+0x1544 == 1) on its paths" holds for in-play only. For a foul stop the result is the same (`MI+0x1588 == 2`, so `dl = 0`), but the source is different. CONFIRMED from Main's decompile (`dec/dec.txt` 154–236).
3. **HUD VA.** The brief and the RE doc name the HUD as 0x140473E20. The tree (`impure_sample/pes_fn_14047F170.cpp`, lines 199 and 206) calls `pes_fn_140A73E20`, and does not call 0x140473E20. The doc's VA does not match the tree's callee list. CONFIRMED.
4. **Oracle.** `oracle.txt` has **14** captures (96 lines, 4–7 lines each): live seq083–090, 095–098, live2 seq108–109. It does not match "20 rows, seq088–095". It has **no output columns** (scene id, card level, restart): `grep -c scene` = 0. The outputs exist only in the doc's prose (seq090 → 0x20008). So it is an input oracle, not a red/green oracle yet. CONFIRMED.

## 2. Decode results

**0x14042D5E0 (CONFIRMED, `animation/pes_fn_14042D5E0.cpp` and the decompile):** `idx = (p3 < 2) ? p3 : 0` (a signed compare, so negative p3 passes through as-is). It copies the 20-byte record at `block + idx*0x14` (two qwords + a dword at +0x10) into the out buffer. Only the top is clamped: p3 ≥ 2 reads record 0. The argument is a block pointer (A1+0x18), not A1.

**0x140A81230 (CONFIRMED structure, `closure/pes_fn_140A81230.cpp`):** args rcx = A1, edx = kind, r8d = side, r9d = key.
- kind == 2 or side == 2 → return 3.
- side == 1 → `n1 = 0x140A80EF0(A1, kind, key, &n2)` = `0x140A80D10(E0, G+0x2AD0, kind, key, &n2)`. Return 0 if n1 > 0 or n2 > 0; 1 if n1 < 0 or n2 < 0; 2 if both 0; else 3.
- side == 0 → `n = 0x140A80CF0(A1, kind, 5)`. Return 0 if n > 0, 1 if n < 0, 2 if n == 0.
- 0x140A80CF0 reads the card counts from **E0+0x750D0** (via 0x140A3E8A0, `lea rax,[rcx+0x750d0]`) and **E0+0x59650**, through the byte lookup 0x1408C9DF0. It also adds its 4th argument (r9d) into one of its two local counts when that is non-zero.
- **OPEN (new):** the decompile's side == 0 call passes only three arguments, so r9d at that call is whatever the caller left in it. A port must reproduce that value. The meaning of return codes 0–3 is INF (names are mine).

**Kind-4 +0x20:** still **OPEN**. Its value is 0 or 1 in every capture (seq084/090 period 3, seq096 period 1, live2 seq108/109). It is not the fouler or the victim slot.

**The `dl` flow (0x140A3A690):** see §1.2. The Z-rule test (`0x140A3A500`) is already ported and tested (FoulOffside4 §1).

## 3. Who calls the tree (binary scan of `pes_full.bin`, E8/E9 rel32, CONFIRMED)

- **0x14047F170** (the tree) is called only from **0x14047E977**, inside **0x14047E940**.
- **0x14047E940** is called from **0x140480A1C** (inside the builder).
- **0x14047E060** is called from **0x1404809C4** (inside the builder).
- **0x14047EA10** is called from **0x140480963** (inside the builder).
- **0x140480910** (the builder) is called from **0x14047FF06**, inside the wrapper **0x14047FEB0** (pdata 0x14047FEB0–0x140480026).
- The wrapper, after the builder, calls `0x141E532E0` and `0x140A3FE70`, does clock arithmetic (`iVar3*3 / G+0x2C0 ->0x5C` and `*6` on the yellow path), and calls presentation: `0x1404511E0`, `0x140A60730`, `0x14102CAD0`, `0x140A60720`, `0x140A607E0`, `0x140A60700`, `0x140445750`.
- **0x140A81230** has more callers (0x14047F75A in the tree, and several in 0x1405xxxxx and 0x1406B5xxx, all outside this pass's scope).

So the tree is reached only through the builder's three variants. Each variant collects the tree's eax into a vector (`47E940`: push; `47EA10`: picks an entry from a 100-slot table via `0x140A92530`, which is random and must go through `pesrandom`; `47E060`: derives its code from the anime record's class byte). The builder then uses `vec[0]` as the scene code.

## 4. 0x140480910: what it writes (CONFIRMED from `dec/dec.txt` and `dec_cardtable.txt`)

- **req (A0)**: +0 = scene code (`vec[0]`), +5 = 1 on the `param_6` path, +0xD incremented when `0x140A3FD20(scene,0)` != 0. 0x20022 is the yellow-card special case (branch `0x140480A8B–0xAC`).
- **Flow (r9)**: the event list at Flow+0x10 (7-dword entries, count at Flow+0xC), and through `14047C500(Flow, A1, …)`: Flow+4, +8, +0x78 (qword, the result of 0x141F5E730/0x141F5E7E0), +0x1D0 = 0, +0x1D8 = 1.
- **r8 (`Flow+0x2790`)**: +0x210 = setne of `0x140A3FE70()`.
- **Globals** through the singleton `0x141F500D0`: +0x5204, +0x520C, +0x5210, +0x5214, +0x5218, +0x515C.
- **A1 is only read.** It is the source argument of `0x141F514D0(obj, A1)`, and the tree's argument. The destination of `141F514D0` is its first argument, so A1 is not its write target. INF: `141F514D0` is untranslated and not decoded.
- **Verdict: 0x140480910 writes no bytes in A1.** The A1 card-table claim in the RE doc is wrong.

## 5. The E0 kind-4 ring writer (partly OPEN)

- **0x140A3DAC0** (`gameplay/pes_fn_140A3DAC0.cpp`, CONFIRMED from the translated body): with blk = gpr[1]: count (+0x1900) = min(count+1, 8); head (+0x1904) = (head+1) mod 8; the record slot is `blk + head*0x320` (the 0x80 bias is the copy loop's); the 0x320 bytes are copied from the source in gpr[2]. Stride 0x320 and count saturation at 8 match the RE doc's kind-4 facts.
- **Only direct caller in the image: 0x140A3DFF9**, which lies inside the event recorder **0x140A3DC40–0x140A3E13D** (pdata). The recorder's code near 0x140A3DFF9 uses a 0x328-stride per-kind block at RBX+0x1C258 and a 64-entry event ring at +0x26FF0 (count +0x271F0, head +0x271F4); for kind RSI == 7 it copies a 0x3C-byte block to +0x180698…+0x18075C. The register meaning (RBX, RSI) is not resolved, and Ghidra splits the body. INF: 0x140A3DAC0 pushes the kind-4 ring from this recorder. OPEN: which block the call targets and how the 0x320 record is filled (the source of the +0x20 field).
- Readers: **0x140A3E150** and **0x140A3E190** (ring count/head, read-only; 0x140A3E150 is called seven times by the tree).

## 6. Why the switch cannot be done faithfully this pass

1. **State GF does not own.** The tree reads 53 spec input keys, plus the card-history reads that the spec leaves out: E0+0x750D0, E0+0x59650, and G+0x2AD0 (for 0x140A80D10). Three writers are still unknown: the kind-4 record fill (§5), the other kind blocks, and A1.18's per-slot records (0x14042D5E0 reads them; the writer is not found).
2. **The callee closure is partly untranslated.** `14159F3E0` (an out-of-set stub), the import slot behind `0x1415A1714`, and the builder's helpers `141F500D0`, `141F514D0`, `141F5E730`, `141F5E7E0`, `141F620D0`, `14047BBD0`, `14047BED0`, `14047BCA0` have no translated body. The tree itself is translated, but the builder that runs it is not. Presentation callees (141794410, 140A73E20, 1414DAB90, 140A3AF90, 140A489D0) are translated bodies called directly from the tree. A stub therefore needs a translator change (`tools/dev/translate/stubs.json`, then regenerate). A runtime skip is not possible without changing the generated tree.
3. **Randomness.** The builder's `47EA10` path picks from a table with `0x140A92530`. GF must route it through `pesrandom.hpp` and seed it the way PES does. Not checked.
4. **Only a partial oracle.** `oracle.txt` has inputs only (14 captures, no outputs). A red test needs PES's output (the scene id from a bit-exact replay of the tree). The RefReplay3 harness has those, but its inputs are full snapshots, which cannot be committed.

Decision: I wrote no GF code. A switch with the A1 or E0 state half-owned would be a half-state the ticket forbids, and a red test with no PES-derived expected value would be invented.

## 7. Numbered open items (next session, in order)

1. **Resolve the recorder (§5).** Decode 0x140A3DC40's kind-4 path with register-level detail (the RBX/RSI meanings, the source record, the 0x320 fill). Use `dec.sh` on the recorder's body ranges plus 0x140A3DA00 and 0x140A3CDD0. Settles the kind-4 +0x20 field too.
2. **Full input closure of 0x14047F170 and 0x140A81230.** Add E0+0x750D0, E0+0x59650 and G+0x2AD0 to the spec's input list (`tools/dev/diffharness/specgen`), then re-run the replay for those keys.
3. **Confirm the r9d value at the 0x140A80CF0 call (§2).** Read the translated 0x140A81230 for what it does with gpr[9] on that path.
4. **Route the builder** (`0x140480910`, `47E940`, `47E060`, `47EA10`, `0x140A92530`) and its untranslated helpers through the translator, or record them as presentation or random stubs with `ponytail:` sites. Decide which helpers write the Flow state the sanction needs.
5. **Presentation stubs** (HUD, menu, cutscene): these are translated bodies called directly, so the translator must regenerate the tree with them stubbed. Do not patch generated files.
6. **Then** build A1/A1.18/E0 in PES layout in the `PesWorld` window (`pesworldrecord.hpp` already owns G, MI and the per-player blocks), add the red/green tests from a PES-derived output oracle, delete `referee.cpp`'s card mapping (lines 780–787, 800–831, 1100–1270) and the `CardBook` advantage path only if PES's late-card step covers it (INF: it does, §9.5 of FoulOffside3), and run the `pes_*` suite and seeds 7 and 3.

## 8. Verification run

- Merge: `git merge origin/main` clean (17 files, merge commit `69466b20`); push confirmed by `ls-remote`.
- Decodes: `dec.sh` on 0x14047C500, 0x14047E940, 0x14047E060, 0x14047EA10, 0x140A80EF0, 0x140A80CF0 (`dec_cardtable.txt`, 270 lines); on 0x140A3DFF9, 0x14047FF06, 0x14047FEB0 (`dec_callers.txt`); on 0x140A3DC40 (`dec_recorder.txt`, 351 lines).
- Scans: `callers.py` (E8/E9 rel32 over `pes_full.bin`), `pdata_entry.py` (`.pdata` lookup).
- Spec check: 53 input keys in `14047F170.json`; no A1+0x6C, no E0 +0x750D0/+0x59650.
- No builds, no tests, no sim runs: no GF code changed. Gate not run (Main gates).
