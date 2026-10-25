# ArgState2: does real state lift impure-function coverage?

Scratch (PES-derived, not committed): `<workdrive>/gf-ArgState2-scratch/`. Runner: `measure3.py`. Data: `measure3/coverage.json`, `measure3/log_*.txt`, `xlate_z/`, `xlate_T0/`.

## Answer

No, not materially. Seeding the real G object and its children from the live snapshot moves median own-block coverage over the 28 specs that run under all three variants from 0.209 to 0.236, and the 50% floor count from 9 to 10. One spec crosses the floor (1404F8C70, 0.106 to 0.574). Seeding argument root A0 with team record T[0] adds nothing on top of G: traces are identical on all 28. G seeding lowers coverage on two specs (14065DAB0, 14097FCE0) and makes one spec (1405AD720) fail.

Two things cap coverage and limit the measurement. 21 of 50 specs fail in the harness before any coverage is recorded. And the case inputs overwrite the argument bytes these functions read, so the seeded bytes are not what the functions see.

## What was fixed and pushed

- **`tools/dev/translate/seed.py`, commit `e01d58b`.** Two bugs:
  1. Each child-slot hop read `read(addr, 8)` from the live G mapping, which is zero-filled (ArgState `probe1.txt`). No children were seeded. G's own slots now come from `g.bin`.
  2. The walker never added the slot offset. It read the G base for every hop instead of `G+slot`.
  Also new: `arg_roots` binds `A0` to `T[N] = *(G + (N+0xA1)*0x28)`. Argument-rooted regions (`A0.6D48.E0`, ...) walk their chain from that root. `--team N` selects the record.
- **`tools/dev/translate/driver.py` and `diffmain.py`, commit `41659b6`.** `--seed-state DIR` applies the seed to the harness corpus (`run.py --seed-state`) and to the translated diff main, which copies the seed bytes into the per-case segments before the args and `regions_init` writes. The translated side is seeded exactly as the harness is.
- PII checks passed on each commit.

## Harness measurement

Variants, all on the impure specs, measured with `driver.block_coverage` on the `--trace-addrs` trace (same method as Main's `measure_seed.py`):

- **unseeded**: no `--seed-state`.
- **G**: `seeds_G`, G shadow plus every child whose chain from G resolves to a captured mapping.
- **G+A0=T[0]**: `seeds_T0`, the G seed plus the A0 root bound to team record T[0].

Results on the 28 specs that run under all three variants:

| | unseeded | G | G+A0=T[0] |
|---|---|---|---|
| median own-block | 0.209 | 0.236 | 0.236 |
| mean | 0.344 | 0.354 | 0.354 |
| >= 0.50 (floor) | 9 | 10 | 10 |

- Per spec, G changes 4 of 28: 1404F8C70 0.106 to 0.574 (crosses the floor), 140639100 0.871 to 0.886, 14065DAB0 0.771 to 0.619 (down), 14097FCE0 0.204 to 0.143 (down). The other 24 are identical.
- G+T0 matches G on all 28 traces (addresses). Case outputs match except 140A57650 (already 1.000) and 14052A820 (fails in both variants).

### Full table (all 50 impure specs)

| spec | unseeded | G | G+A0=T[0] | blocks (unseeded / G / G+T0) |
|---|---|---|---|---|
| 0x1404386D0 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1404405D0 | 0.072 | 0.072 | 0.072 | 8/111 / 8/111 / 8/111 |
| 0x1404F47A0 | 0.400 | 0.400 | 0.400 | 8/20 / 8/20 / 8/20 |
| 0x1404F6C30 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1404F8C70 | 0.106 | 0.574 | 0.574 | 30/284 / 163/284 / 163/284 |
| 0x1404FA720 | 0.136 | 0.136 | 0.136 | 3/22 / 3/22 / 3/22 |
| 0x1404FB2A0 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1404FE9C0 | 0.214 | 0.214 | 0.214 | 47/220 / 47/220 / 47/220 |
| 0x1404FEDC0 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x140503430 | 0.619 | 0.619 | 0.619 | 146/236 / 146/236 / 146/236 |
| 0x140504D20 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1405055E0 | 0.848 | 0.848 | 0.848 | 28/33 / 28/33 / 28/33 |
| 0x140509A40 | 0.045 | 0.045 | 0.045 | 3/67 / 3/67 / 3/67 |
| 0x140510D30 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x14051B8B0 | 0.058 | 0.058 | 0.058 | 7/120 / 7/120 / 7/120 |
| 0x14051CF70 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x140529490 | 0.174 | 0.174 | 0.174 | 8/46 / 8/46 / 8/46 |
| 0x14052A190 | 0.639 | 0.639 | 0.639 | 23/36 / 23/36 / 23/36 |
| 0x14052A820 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x14053B110 | 0.259 | 0.259 | 0.259 | 7/27 / 7/27 / 7/27 |
| 0x14053B420 | 0.200 | 0.200 | 0.200 | 8/40 / 8/40 / 8/40 |
| 0x14057A3E0 | 0.286 | 0.286 | 0.286 | 4/14 / 4/14 / 4/14 |
| 0x14057BA50 | 0.545 | 0.545 | 0.545 | 6/11 / 6/11 / 6/11 |
| 0x14057D3A0 | 0.172 | 0.172 | 0.172 | 10/58 / 10/58 / 10/58 |
| 0x1405A6DE0 | 0.039 | 0.039 | 0.039 | 6/153 / 6/153 / 6/153 |
| 0x1405AD720 | 0.370 | FAIL | FAIL | 27/73 / rc=1 / rc=1 |
| 0x1405CCD50 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1405CE4F0 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1405CE930 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1405DC350 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1405DE950 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x1405E4460 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x140601110 | 0.312 | 0.312 | 0.312 | 24/77 / 24/77 / 24/77 |
| 0x14062E240 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x140639100 | 0.871 | 0.886 | 0.886 | 61/70 / 62/70 / 62/70 |
| 0x1406486D0 | 0.007 | 0.007 | 0.007 | 2/289 / 2/289 / 2/289 |
| 0x14064DC50 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x14064DE60 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x140654E60 | 0.760 | 0.760 | 0.760 | 79/104 / 79/104 / 79/104 |
| 0x14065C2A0 | 0.058 | 0.058 | 0.058 | 7/120 / 7/120 / 7/120 |
| 0x14065DAB0 | 0.771 | 0.619 | 0.619 | 81/105 / 65/105 / 65/105 |
| 0x140687310 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x14080EBA0 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x140975710 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x140978E70 | 0.090 | 0.090 | 0.090 | 15/166 / 15/166 / 15/166 |
| 0x14097CD00 | FAIL | FAIL | FAIL | rc=1 / rc=1 / rc=1 |
| 0x14097FCE0 | 0.204 | 0.143 | 0.143 | 10/49 / 7/49 / 7/49 |
| 0x14097FFF0 | 0.062 | 0.062 | 0.062 | 18/291 / 18/291 / 18/291 |
| 0x140A539F0 | 0.692 | 0.692 | 0.692 | 36/52 / 36/52 / 36/52 |
| 0x140A57650 | 1.000 | 1.000 | 1.000 | 7/7 / 7/7 / 7/7 |

The 1404F6C30 row uses the scratch copy with the invalid input removed (see below). Its original spec fails earlier.

## Why 21 specs fail in the harness

- **1404F6C30 (stack input).** Input `stk5095` has `stack_offset` 0x9F58. `set_inputs` writes it at `stack_top + 0x9F58`, above the 128 KB stack region, which gives `UC_ERR_WRITE_UNMAPPED`. The offset comes from spec generation (`0x20 + 8*k`, k=5095), so it is not a valid stack argument. I did not change the shared harness. The scratch copy without that input still fails with `UC_ERR_MAP`.
- **20 of 21 unseeded failures** fail with `emu error: Invalid memory mapping (UC_ERR_MAP)`. `run.py` aborts the whole spec on the first failing case (`return 1`), so no trace is written and the spec gets no coverage. The region layout is valid (checked for overlaps among spec regions, stack, RET sentinel, and the image extent). The root cause is not isolated. `run.py` has no `mem_map` or `mem_unmap` call during emulation, so the error comes from something I did not trace.
- **14080EBA0** fails with `Invalid memory fetch (UC_ERR_FETCH_UNMAPPED)` at RIP 0x1415A17CE, which is in the stub area.
- **1405AD720** runs unseeded (27/73), but under the G seed a guided case (23) fails with the same `UC_ERR_MAP`.

So the floor counts above cover the 28 specs that run under all three variants. The other 22 are unmeasured.

## Translated side (driver with `--seed-state seeds_T0`)

Full 50-spec driver runs, seeded and unseeded (`xlate_T0/`, `xlate_z/`). Per the driver, `diffed` means bit-exact **and** own-block coverage at or above the 0.5 floor.

| | seeded (T0) | unseeded |
|---|---|---|
| translated and diffed (bit-exact, >= floor) | **9** | 8 |
| bit-exact, below floor (not diffed) | 18 | 19 |
| harness closure gap (`UC_ERR_MAP`) | 18 | 17 |
| unmodellable callee | 4 | 4 |
| mismatch | 1 (140503430) | 1 (140503430) |

The 9 seeded floor specs: 1404F8C70 (0.574), 1405055E0, 14052A190, 14057BA50, 140639100, 140654E60, 14065DAB0, 140A539F0, 140A57650. Unseeded has the same set minus 1404F8C70, which the harness cannot run without the seed.

The one mismatch (140503430) appears identically in both variants: at case 123 the harness returns `rax = 0x22800000` (a synthetic region base) and the translated code returns `rax = 0x1001FF00` (the stack top). That is a pre-existing harness-vs-translation difference, not something the seed causes. It blocks 140503430 from being diffed.

## Caveats

- Only A0 was bound. The argmap also names man/anime/slot records and A1..A3. Those roots stay zero, so "argument objects" were tested only for team record T[0].
- Live team records have null `+0x6D48` and `+0x6D50` child slots (T[0] and T[1]), so the spec's `A0.6D48` children stay synthetic. The A0 seed is the team record alone.
- A0 aliases the G child `IMG.1436F3DC8.1928`, which is also T[0]. Each gets its own copy, so writes do not propagate between them. This is an approximation.
- Why argument seeding does nothing (inference, not measured directly): the spec's inputs cover the bytes these functions read, and each case writes them over the seed. The unread bytes never affect the traced paths.

## Next steps

1. Find the source of `UC_ERR_MAP` in `run.py` emulation (20 specs, plus the translated closure gaps). It is the largest gap in the floor count.
2. Bind the man/anime/slot records per argmap, and A1..A3 where the spec names them.
3. Investigate the 140503430 return-value mismatch (harness synthetic base vs translated stack top).
4. Investigate why the G seed makes 1405AD720 fail, and why it lowers 14065DAB0 and 14097FCE0.


## Main's reading

The experiment did not test real state: each case's synthetic inputs are applied after the seed and overwrite the fields the functions read, so they run on random values either way. The snapshot also raced the end of a match (team records' children already null). A valid test draws the cases from real instances (values perturbed around the real ones) and, ideally, a snapshot taken mid-play.
