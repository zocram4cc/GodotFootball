# PES ActionPress chain, PressLane2 (updated): driver proof merged and pushed (8f0d8192); the ctx/owner writer is still OPEN, and the static zeroing contradicts the entry captures

Branch `PressLane` pushed: `8f0d8192` (`git ls-remote origin PressLane` confirms). It contains the driver merge and the regenerated layer. The walk in `_BallControlCommand` is NOT deleted; ActionPress is NOT wired. Next blocker: A2+0x38 (ctx) and A2+0x40 (owner) are written between the per-X tick and the driver, and I cannot name the writer.

## 1. Committed (8f0d8192)

- `prove_bounded 0x1405141F0 --tag pressdrv --real pid1408046/seq33805 --real pid1858437/seq1380 --real pid1858437/seq9828 --merge`: **3 of 3 calls bit-exact, 0 mismatches**, own-block 87/139 (0.6259). Written to `results_trapaction.json` and `closure_pressdrv.json`.
- Layer regenerated: 10966 functions (3902 recorded bit-exact, 7064 closure). New closure members `0x1409621D0`, `0x140962250`.
- `gen_translated.py --check` on the tip: rc 0, "regenerate byte-identical" (the first check failed only because the two new closure files were untracked; now tracked).
- `check_pii` run from `<workdrive>/GodotFootball`: rc 0. The changed files grep clean of `<workdrive>` and `<home>`.
- Scratch: the unpacked seq33805/seq1380/seq9828 copies are deleted (live2 originals untouched, still packed). `work-drv3` (26 MB) remains.
- Not run: ALiveBall, PesStallQuick, ankle 9/9, PesHang (no behaviour change; the walk is untouched).

## 2. Chain (CONFIRMED, from the real stack of seq33805; unchanged from the earlier report)

tick `0x140928FC0` (rcx = X) -> `0x1409257B0` -> `0x140929A00` -> man `0x14092A340` (rdx = A2) -> `0x1409F5240` -> loop `0x1405140E0` (rdx = A2) -> `vt[0x10]` wrapper `0x140961FB0` (rcx = A0) -> driver `0x1405141F0`. A2 = X+0xACE8, A0 = X+0xFE70.

## 3. The contradiction (CONFIRMED by the new relaunch captures, pid2282538)

- The tick's init (`0x14092910C` path, `0x1409290F2` / `0x1409290F6`) stores 0 to A2+0x40 and A2+0x38, with `r14 = 0` (no other writes to r14 in the function).
- Yet the entry states show the SAME nonzero values at every point after the tick's init:

| capture | hook | A2 | slot | ctx (A2+0x38) | owner (A2+0x40) |
|---|---|---|---|---|---|
| seq3076 | pt_140928FC0 (tick entry) | 0x33701588 | 21 | 0x32EC15D0 | 0x3C8B0320 |
| seq008 | pt_14092A340 (man entry, rdx = A2) | 0x33701588 | 21 | 0x32EC15D0 | 0x3C8B0320 |
| seq2956 | pt_1405140E0 (loop entry) | 0x33701588 | 21 | 0x32EC15D0 | 0x3C8B0320 |
| seq2988 | pt_1405140E0 (loop entry) | 0x33701588 | 21 | 0x32EC15D0 | 0x3C8B0320 |

  Other tick captures (seq5518, seq8188) and man (seq12207) carry other A2s with nonzero ctx and owner, all at tick/man entry.

- So either (a) the zeroing in the tick is undone before the man entry by a writer I have not found, or (b) the zeroing does not happen in these runs. The static check found no store to A2+0x38 / +0x40 with any base register in: `0x140929A00`, `0x14092A340`, `0x1409F20C0` (its rdx-stores go to X+0x6C0..; it reads A2 into r13), `0x14092F5B0`, `0x14092E7E0`, `0x140927470`, `0x140699610`, `0x1409F4DB0` (its rdx-stores go to X+0x894), `0x14092BBB0` (stores are rsp-based), `0x142106110`, `0x14092F390`, the wrapper `0x140961FB0` (writes only A0+0x48, A0+0x58), the loop `0x1405140E0` (its rdx-stores go to rsi+0x1988), and the translated driver. Pointer-arithmetic stores (a base computed into another register) are NOT ruled out.

## 4. Next (for Main to arm; one hook settles it)

1. `pt_1409257B0`: 0x1409257B0 (frame 6, rdx = A2, any call), EVERY 1000, cap 3. If ctx is zero at its entry, the writer is in the tick's own code between the init and frame 6 (`0x140928FC0`, 0x14092910C..0x1409294E4, which includes the calls `0x140A3E7D0`, `0x140A60530`, `0x14092ED30`, `0x14092F5B0` region). If nonzero, the zeroing is not reached in those runs.
2. If (1) shows zero: a mid-tick hook right after the init store (`0x14092910C`, displaced bytes from `hooks_for_image`) to see the ctx after the init and before the calls.
3. Only then translate and prove the tick and its chain bottom-up (none of the eight chain functions is translated), then `RunActionPress(slot)` through the tick, and the red test.

## 5. Corrections

- The owner doc's A2 writer statement is wrong: it is not G (`0x140963501`). G is read-only on A2 (the chooser `0x1409634B0`'s tail).
- A0 = X+0xFE70 (= M+0xF518), not X+0x10E70.
