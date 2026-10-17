# PES 2021 Extra Frontman: FC860 candidate scores and the +0.2 (EFPin, 17-10-26)

Scope: Main ruling. Measure the +0.2 from FC860's candidate inputs (base score per candidate, winner), with and without bit 15 on the side-1 CB; confirm side-1 slot 1 is a CB; restore and verify. Detail: `pes-re-extra-frontman.md` §6.10.

Tags: CONFIRMED = decompile, raw disassembly, or live read/write/readback in this pass. INF = inferred. OPEN = blocked.

## 1. Results

| Item | Result |
|---|---|
| Base score (reach, mode 2) | CONFIRMED formula `C1 + K1·cJ + (C2−C1)·cH + D2`, live constants 1.935 / 0.435 / 2.335 / −0.054343. H = copy+0xA1, J = copy+0x9C. |
| CB base (side 1, slot 1) | 2.3131 in every window (H 185, J 67). |
| CB score, no bit (A) | 2.3131 (80 windows). |
| CB score, bit 15 (B) | 2.5131 (138 windows). **+0.2000 exactly**, gate read live. |
| Bit-15 gate | `roster bit 15 AND DAT_142613D20[role] bit 15`. Only role 1 (CB) has bit 15 in its row (`0xD000`), so the bit can only select a CB. |
| Side-1 slot 1 is a CB | CONFIRMED: `T1+0x4C00[1] = 1` (position enum 1 = CB). Slot 0 is GK (0). |
| Gap to next eligible candidate | 0.0738 (2.3131 vs 2.2393) < 0.2. |
| Does the bit change the winner (gap < 0.2)? | **Not answerable live.** The decoded winner is the CB with and without the bit. The live winner is 255 (none) in all 218 windows, A and B. |

## 2. Why the live winner disagrees (OPEN)

The decoded fallback (L102–166) picks the CB in every window. Live, the designated slot is 255 at close in all 218 windows of this pass. Earlier 300-s runs (§6.9) did show the CB in 12% of windows, so the live winner depends on state the decode does not model.

- **Unmodelled gate:** the kind-8 exclusion `FUN_140A63950(X, side, 8, u)`. `X` is the FC860 local `+0xF8`. In `FUN_14051B8B0` the same call uses `SP+0xB98`, so `X` may be `ctx+0xB98`, giving `[X+0x18] = [brain+0xBC0]`. That pointer is null in every live read, and `[brain+0xBB0]` gives float data. The table is not located.
- **Record flag:** `FUN_140A3AF90` takes its per-slot branch only if `T[0]` equals the side flag. Side 1 has `T[0]=0`, so every side-1 candidate gets the same record. The record fields read 0 for both sides. This affects the tie-break and the non-bit filter. The filter was inactive in this pass (`P+0x7B5D = 4`).

## 3. Side-1 slot 1 is a CB (CONFIRMED)

`T1+0x4C00[k]` for k = 0..10: `(0, 1, 1, 2, 2, 3, 4, 6, 7, 7, 9)`. Side 0 has the same shape. The enum (0 GK, 1 CB, 2 LB/RB, 3 DMF, 4 CMF, 5 LMF/RMF, 6 AMF, 7 SS, 8 LWF/RWF, 9 CF) is INF from `pes-re-matchlogic.md` §352 and `pes-re-stamina-subs.md`.

## 4. Runs

- `score_run.py A 90` and `score_run.py B 90`, side 1. A: b8a2 pinned to 1 only. B: b8a2 pinned to 1, plus bit 15 on the CB (roster gate word `0x1D4A9590`, copy word `0x1D4DE8D0`). Designated object `0x1D49CCA0`, 0xFF written once per window.
- A: 80 windows. B: 138 windows. Read errors (EIO, transient when the object pointer toggled): 136 in A, 166 in B, all skipped windows.
- Logs: `gf-EFPin-scratch/score_A.log`, `score_B.log`. Analysis: `analyze_scores.py`.

## 5. Restore (verified)

- A: `b8a2` 0x00 (readback ok); designated `+0x3EF8` 0xFF (readback ok).
- B: `b8a2` 0x00, roster 0x00, copy 0x00 (readbacks ok); designated 0xFF (ok).
- No pin is active. `MI+0x1168/0x116C/0x1574` were never written.

## 6. Open

1. Locate the kind-8 table (FC860 local `+0xF8`). This is the gate that makes the live winner differ from the decode.
2. Fix `FUN_140A3AF90` for side 1 (`T[0]` flag) and read the real record fields.
3. Only then can the bit's effect on the winner (gap < 0.2) be measured live.
