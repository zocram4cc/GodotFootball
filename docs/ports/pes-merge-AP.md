# Batch AP merge

**Landed 30-10-26.** Lane `MergeBatchAP` = main + EndClear `af4e8388` + KickSoftMin `eb200a6d` + test fix APFix `7f6e9b35`.

## Contents

- **EndClear**, which also carries NativeSeam `970a8d47`:
  - **Native heap seam:** `PesHeap`, selected through the global stub table's `native_services`.
  - **Translator:** loud untranslated tail jumps, and libm `logf`/`powf` models.
  - **Danger flag:** GF takes it from PES's translated danger builder `0x142119390`, writer `0x140699960` and pressure builder `0x14069A500`.
  - **Carrier walk:** PES's translated walk `0x14210F110` decides the carrier's unit, forced clear included. The GF walk and its rule helpers are deleted. The carrier block is 0x4300 bytes, MI 0x1780, MA 0x26040.
  - **Unit-info and state updaters:** they run on GF's window. The none-event record is built the way PES's constructor builds it.
  - **Install tables:** tables PES reads by register (anime class, sine, frame-rate cells, tier) are loaded from the exe (`pesimagetables`).
  - **Bugs fixed:** the designated taker was written to the wrong record, and a GF write zeroed PES's own danger byte.
- **KickSoftMin:**
  - PES kick-quality fold, plus the in-flight kick branch, bit-exact on 510 rows.
  - **Bug fixed:** a ceremony throw-in taker's ball was not pinned to the hand.
  - Seeded tests re-staged by seed search.
- **APFix:** `test_tail_jump_loud.py` builds a synthetic PE with `.pdata`; batch AO's extent rule replaced the decode window.
- **Recorded-check caps:** `diff_cutscene_recorded` and `diff_presentation_recorded` raised to 4,800 s. They need 2,855 s and 2,317 s at load ~30.

## Gate: fast tier, the first under `GATE_TIER`

A runtime header changed, so the selector re-checked every recorded function, about 5 h at load ~30.

| Check | Result |
|---|---|
| Layer on the merged tree | `gen_translated.py --check`: 4,346 functions regenerate byte-identical |
| Ledger | current |
| Recorded selection | 245 + 685 + 546 + 2,253 + 34 functions; all bit-exact except the presentation set, which hit its old 2,400 s cap; the same check run directly: 546/546 bit-exact (2,317 s) |
| ctest | 2 red: `diff_presentation_recorded` (that timeout) and `translate_tail_jump_loud` (merge interaction, fixed by APFix; then 3/3 with the closure-walk and extent tests) |
| Smokes | 2-min seed 7, both rc 0: switch-off `e663d1e917d6c4f4` (33,628 ticks), pes-loop `980c54e27f93ced2` (25,942 ticks); new baselines |

Final merge of main: docs only.
