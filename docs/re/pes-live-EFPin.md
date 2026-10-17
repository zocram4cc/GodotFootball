# PES 2021 Extra Frontman: live pin runs and the designated-slot base (EFPin), 17-10-26

Scope: owner ruling (Main, 17-10-26): side 1 (CPU side), pin `team+0xB8A2`=1, set side 1's designated slot to 0xFF, run A (b8a2 only) and B (b8a2 + bit 15 on a side-1 CB), ~5 min each. Plus: decode the entry gate of `FUN_14051D9E0` / `FUN_1405EC600`. Never written: `MI+0x1168`, `MI+0x116C`, `MI+0x1574`. Detail: `pes-re-extra-frontman.md` §6.9.

Tags: CONFIRMED = decompile or live read/write readback in this pass. INF = inferred. OPEN = blocked.

## 1. Correction to the earlier EFPin report (CONFIRMED)

The earlier runs (§6.8 of the frontman doc) read the designated slot from the wrong object. The team tick's ctx is `brain+0x10`, where `brain = [H+0x450+8s]`, `H = [0x1436F3F98]`. So the designated slot is `P = [ctx+0xBA8] = [brain+0xBB8]`, and the ordinal list is `SP[4+j] = brain+0x20+4j`. The earlier code read `[brain+0xBB0]`, which is a different per-side object that FC860 never writes. Every earlier "P+0x3EF8 = 0" and "FC860 never ran" result is void. Those writes went to the wrong object and were restored.

## 2. Entry gate of the tick (CONFIRMED decompile)

- `FUN_14041EC80` (loop at `0x14041EDC0`–`0x14041EDE4`) calls `FUN_1404F6C30(brain_s, …)` for s = 0, 1 whenever `brain_s = [H+0x450+8s] != 0`. There is no side test.
- `FUN_1404F6C30` calls `FUN_14051D9E0(brain+0x10, …)` directly (L243, when the byte at `lVar15+0x1775` == 0x17; `lVar15` presumed MI, not checked), or via `FUN_1404F9DC0(brain)` → `FUN_14051D9E0(brain+0x10, …)` (L62 or L213).
- `FUN_14051D9E0` calls `FUN_1405EC600(ctx, 0)` in the `MI+0x116C==1` branch and `(ctx, 1)` in the `MI∈{2,3}` branch (`0x14051DE03`, `0x14051E264`). Neither is side-gated.
- `FUN_1405EC600` has one `return` (L497). Its 11-slot loop calls FC860 on the 11th pass.
- Live (read-only `ptr_probe.py`, 31,120 samples): `[brain+0xBB8]` is non-null in ~6% of samples. When non-null it holds 0xFF for side 0 (734 samples) and side 1 (1,212 samples). `b8a2` was not read in that probe; the baseline had `b8a2`=0 on both sides. 0xFF is the gate-1 signature of FC860 (`b8a2`==0 → `des=0xFF`). FC860 runs for **both** sides (likely; the probe did not read `b8a2` in the same samples).

**Human-side reason: NOT CONFIRMED.** The only side test is `FUN_140A55CF0(G+0x180, side)`, used in `FUN_14051B8B0` (L97, the bit-15 store, branch `==0`), `FUN_1405F63C0` (L145, called with one argument, so unreliable) and `FUN_1405F7D00` (L30). Live: `mode(DAT_143704E38+0xF0)=8`, table 6 (not 10). Two live records: rec0 (type 0, id 0), rec1 (type 2, id 1). **Both `FUN_140A55CF0(…,0)` and `(…,1)` return 1**, so the `==0` branch is not taken for either side. The test does not separate the sides here; what the record `id` and `type 2` mean is OPEN.

## 3. Side-1 runs (CONFIRMED live)

Designated object (side 1): `0x1D49CCA0`. Pinned words: side-1 `T+0xB8A2` (`T = G+0x1928+0x28`) at `0x1D49B7B6`, 0→1 (both runs). B also: side-1 CB roster gate word `0x1D4A9590` (0x0→0x8000) and copy word `0x1D4DE8D0` (0x0→0x8000). CB = global 12 = side 1 local slot 1. "CB" is INF (formation role, not checked live). Per window: write 0xFF to `+0x3EF8` once if not already 0xFF, log to close.

| Run | Windows | `b8a2_1` at open | Open des | Close des = 12 (CB) | Recompute to CB |
|---|---|---|---|---|---|
| A (control) | 914 | 1 in all | 0xFF 831, 12 83 | 104 | 104 / 831 = 12.5% |
| B (cb) | 502 | 1 in all | 0xFF 425, 12 77 | 69 | 69 / 425 = 16.2% |

- Pooled: z ≈ 1.8 (two-sided p ≈ 0.07). Not significant.
- Candidate list SP[4..14] (side 1): `(0,1,2,3,7,4,5,6,8,9,10)` in 914/914 A opens and 495/502 B opens. B also had 5 all-255 (cleared) lists and 2 rotated lists; A had none. Not attributed.
- Pin held: `held_all_pins` 0.8% (A), 1.6% (B). Reverts are frequent; at every window open `b8a2_1` was 1, so the pin held when FC860 ran.
- Restore (readback verified, `ok=True`): `b8a2` 0x00, roster 0x00, copy 0x00, designated `0x1D49CCA0+0x3EF8` 0xFF (first-seen value).

## 4. Answers to the ruling

1. **Does `P+0x3EF8` (side 1) name the CB in B and not in A?** **No.** The CB is named in A too (12.5%) and in B (16.2%). Bit 15 on the CB is not what makes him the designee.
2. **How often?** A: 104 of 831 recompute windows; B: 69 of 425.
3. **Side-1 candidate list:** unchanged in A and B; the CB is at index 1 in both. The bit-15 store `FUN_14051B8B0` L106 is not reached (side test returns 1). Consistent with the list not changing.
4. **+0.2 effect:** **not attributable.** The CB is named without bit 15, so the +0.2 cannot be separated from the designation. Scores not logged.
5. **`F63C0` / `F7D00` outcomes:** not observed (no breakpoints; outputs not in memory after return). OPEN.
6. **Human side:** OPEN (see §2). The side test does not separate the sides in this match state.

## 5. Other void results

The ordinal-list values and "cleared SP lists" in the frontman doc §6.3, §6.6 and §6.8 use the same wrong base. Re-derive before citing.

## 6. Open items

1. Confirm side-1 local slot 1 is a CB (roster record and formation role).
2. Measure the FC860 candidate scores (breakpoint or decode of the fallback `record` fields) to test the +0.2 directly.
3. Decode the meaning of `FUN_140A55CF0`'s record `id` and `type 2`, to find the human-side marker.
4. Side 0 has not been run with the corrected base (side 0 designation during pins is unmeasured).

## Files

Scratch: `<workdrive>/gf-EFPin-scratch/`: `win_run.py` (side-1 runs), `ptr_probe.py` (base probe), `side_probe.py` (side test), `summarize.py`, logs `win_A.log`, `win_B.log`, `ptr_probe.out`, decompiles `d9e0.c`, `f6c30.c`, `f9dc0.c`, `ec600.c`, `ec80_dis.txt`. No repo writes.
