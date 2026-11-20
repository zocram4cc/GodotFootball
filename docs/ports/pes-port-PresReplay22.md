# pes-port-PresReplay22: the seq247 hang was a live-in classification bug; the copy test runs over all eight real copies; the frame-table adjustment is removed (round 21)

Branch `Presentation`, pushed `a0243cde..b9bf414e` (`git ls-remote origin Presentation` = `b9bf414e23b970ed34a97c686f2d3800c028160c`). Build dir and unpacked captures removed (scratch 12 MB).

Labels: CONFIRMED = a PES replay (`frame_trace.run_recorded`), a test run, a gdb backtrace, or a scan; INF = inferred; OPEN = not settled.

## 1. seq247 hang: cause and fix (CONFIRMED)

- gdb (copy test on seq247, interrupted): the loop is `141E96380` (the list walk: `gpr3 = [gpr3]` until the sentinel `[X+0x30]`, with the copy gate `1408BDF70` per node), under `141EBF220 <- 14217DB10 <- 14217DEF0`.
- PES's run of the same frame (walk.py, reads inside `141E96380`): the nodes are a regular array (stride 0x27D0), 66 reads in the walk, 30 pages. Two of the walk's pages were missing from the live-in list: `0x34793000` and `0x1d4ff000`.
- pagewrites.py on `0x34793000`: the frame first **writes** `0x347934d8` (allocator words, from `1403FD6F9`/`1403FD8C5`) and only later **reads** `0x34793da0`/`0x34793d90` (the node links) — a node the frame never wrote. The page-level live-in rule (`first access == R`) classed the page as written, so the port's copy of it was zero. A zero link makes the walk read address 0 forever: the hang.
- Fix (`tools/dev/translate/copy_livein.py`): live-in pages are the pages of every byte the frame reads before writing that byte (per-byte written set). seq247's live-in grew from 261 to 264 pages; the other captures grew too (e.g. seq205 290 → 292).
- seq247's copy test now passes.

## 2. Every real copy runs green (CONFIRMED)

`RealCopies` (parametrised, `tests/pes_replay_human_test.cpp`) runs over every copy_livein directory under `GF_PES_COPY_CAPTURES`:

| capture | human / palette / frameIndex |
|---|---|
| pid2024691 seq205 | 0 / 0 / 174 |
| pid730995 seq230 | 4 / 4 / 24 |
| pid730995 seq231 | 5 / 5 / 24 |
| pid730995 seq232 | 6 / 6 / 24 |
| pid730995 seq233 | 7 / 7 / 24 |
| pid730995 seq247 | 21 / 21 / 24 |
| pid2402670 seq19556 | 20 / 20 / 74 |
| pid2402670 seq20463 | 10 / 10 / 291 |

- Each instance: `CopiesThePaletteEntryTheHumanSelects` (rec.bytes == after.bin, the release state) and `ASequenceKeepsTheCopiedEntryAcrossTheReadsThatDoNotCopy` (absent and inactive reads keep it). 16 instances, all PASS.
- Without `GF_PES_COPY_CAPTURES`: one instance per test, SKIP with the named reason; the other five synthetic tests pass.
- pid2402670 seq19557 (a second read, R+0x48 != 0, palette unchanged) is not a copy and is not in the set.
- Release test (`AChangedEntry...`, flipped pid2998104/seq567 state with `GF_PES_RELEASE_CAPTURE`): PASS.
- Full run with captures: 22 PASS (16 + 5 + release). HUD 4/4, native service 7/7.

## 3. The frame-table adjustment: not observed, removed (CONFIRMED)

- The adjustment (`kUnitFramesOff = kG2 - frameIndex * 0x1FB0`) was added in round 21 for the palette-byte address. With the adjustment removed (the table is the window), **every instance stays green**, including seq19556 (frameIndex 74), seq20463 (291) and the flipped state (550). So no test observes it.
- Per the owner's rule (no observer: remove it), it is removed (`src/pes/replay_human.cpp`), with its constant.
- OPEN, stated plainly: the palette byte the copy reads is still correct with the table unadjusted and a nonzero key index, and I did not establish the mechanism (the byte address `kG2 + frameIndex * 0x1FB0 + …` is outside every window). The key word is observed (section 4). The next probe, if needed: trace the palette-byte read address in the port (PesMem::rd for an address outside the windows).

## 4. Mutations (run, rebuilt, reverted)

1. **Key word carries no frame index** (`PutU32(k, 0, 0)`): the real-copy suite FAILS: 15 of 17 instances red (every instance whose frameIndex is nonzero and whose palette differs from index 0; the seq205 instance stays green because its palette index is 0). Reverted; green.
2. **Frame-table adjustment removed**: no instance red (section 3). Kept as the observability record; the adjustment itself is removed.

## 5. Region-core closure members: named gap (CONFIRMED)

- `closure_destr14217.json` keeps `14038E6F0`, `14038DF60`, `14038F320` and their callees. The port reaches none of them (their only route, `14037FDB0`, is a boundary service).
- Recorded in the record `tools/dev/translate/results_presentation.json` (14217DEF0) as `"gap": "GAP-REGION-CORE …"`: they are translated and abort loudly if reached (a call from the layer to a non-member aborts). Pruning needs the port-side reach of the closure, which the proof does not record.

## 6. Checks

- `gen_translated.py --check`: 5868 functions (3769 recorded bit-exact, 2099 closure), byte-identical.
- `ledger.py --check`: current (44459 rows).
- Python tests: not rerun this round (no changes to the Python translator logic beyond `copy_livein.py`'s live-in rule; its output checked by the copy suite).

## 7. Commit

- `b9bf414e` on `Presentation` (pushed; ls-remote confirmed). Paths: `src/pes/replay_human.cpp`, `tests/pes_replay_human_test.cpp`, `tools/dev/translate/copy_livein.py`, `tools/dev/translate/results_presentation.json`.
