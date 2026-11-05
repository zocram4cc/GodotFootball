# Batch BG: SimMemory2 (merged to main)

**Contents:** branch `SimMemory2` (`812e0da5`), merged onto main as `532e4949`. Since the branch point `784ffd5d`, main gained only docs, `tools/` and new test registrations in `tests/CMakeLists.txt`, so the gate verdict holds. Report: `pes-gate-SimMemory2.md`.

**Change:**
- PES clip keyframes are released once a clip is prepared, and rebuilt from the PES motion text on first read.
- Clip preparation keeps its order, so variables and indices are unchanged.
- Player-line position caches are built on first request instead of for every clip at match construction.
- No data is dropped: rebuilt keys equal a fresh load, whatever the access order.

## Memory

Peak VmRSS, seed 7, 2 min, through `simslot`:

| Mode | Before | After |
|---|---|---|
| switch-off | 2,366 MB | 313 MB |
| pes-loop | 2,382 MB | 354 MB |

- Keyframes were about 1.9 GiB of the 2.26 GiB.
- A switch-off match rebuilt 3 of 8,838 clip sides; a pes-loop match rebuilt 184.
- The decoded GANI set (146.5 MiB) is now the largest remaining owner; lazy GANI decode has not been started.

## Proof

- **Smokes:** switch-off `399e180f39459c2b`, pes-loop `85c7d7c515cf92ae`, identical to main.
- **`GATE fast: GREEN`:** 2,324 of 2,324 (re-run by Main on slots 5 4; the first run sat blocked behind old-script gates).
- **`pes_clips_test`:** `ReleasedClipsHoldLessHeapThanTheirKeys` is red on main and green after. `RebuiltKeysEqualAFreshLoadOfTheMotion` and `RebuiltKeysDoNotDependOnAccessOrder` pass before and after.
- **Mutations:** M1 (drop the mirror reflection on rebuild) is caught. M2 (remove the key guard in `GetKeyFrame`) is unconfirmed: its run printed no verdict.

## Left

- Re-run M2 with the exit code captured.
- Lazy GANI decode (146.5 MiB).
- The slot count stays 6. Memory no longer binds, but CPU does (load ~25 on 16 cores while gates run). `simslot.sh` keeps its MemAvailable check, because binaries built before this batch still hold 2.3–3.7 GiB per sim.
