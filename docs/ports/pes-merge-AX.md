# Batch AX merge

Lane `MergeBatchAX` = `origin/main` + `GateSpeed` (`af783a4e`) + one lane commit.

## What lands

- **Object cache in the recorded re-check (`driver.py`, `buildcache.py`):** each closure TU compiles to its own object, in parallel. Objects are reused across functions and runs, keyed by compiler, flags, TU content and included headers.
- **PES reference-corpus cache:** `run.py`'s outputs are reused only while every module and data file it loads is unchanged; the dependency set is recorded when the entry is made and re-validated on reuse.
- **Job count:** a jobs knob for the five diff tests, chosen by `gate.sh` from cores and load, and a ctest COST so `diff_core_recorded` starts first.
- **Proof (GateSpeed):** the full core re-check took 2 h 58 min uncached, 2 h 00 min cold and 7 min 54 s warm. All 2,253 verdicts are identical to the uncached baseline. Killing tests in `test_buildcache.py`, plus end-to-end mutations.
- **Lane commit:** `select_recorded` now treats `driver.py` and `buildcache.py` as harness, so a change to the check driver or its caches re-checks every recorded function. Before this, GateSpeed's own gate selected 0 rows.

## Gate

`GATE fast: GREEN`, 8,374 s wall, cold cache, 2 diff jobs (load 11.8). Because the driver changed, the gate re-checked every recorded row in all five sets with the new driver: 245 + 685 + 546 + 2253 + 34.
- ctest: 2280 of 2280 passed.
- No unrecorded status regression (18595 rows compared).
- Off-path ports: 125, all allow-listed.
- Ledger current.
- Smokes unchanged: `3b73c968b4a377e0` / `0e480b158cf80bd4`.

For comparison, batch AW's full re-check gate took 10,558 s for `diff_core_recorded` alone.

## Same period (main, outside the batch)

- `capz.py` packs full captures with zstd (about 3.7x, checksum-tested) and unpacks them sparse. `auto_slim --pack-after` packs kept captures. Free space went from 322 to 680 GiB.
- `rv_watch.py` sweeps finished rendezvous files. The run directory had grown by three files per hooked call (130k in 4.5 h), and the watcher's rescans throttled PES. The match now runs at about 330 frames per 10 s, against 60–70 before.
