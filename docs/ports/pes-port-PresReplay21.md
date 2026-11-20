# pes-port-PresReplay21: the fixture fed the frame the wrong key word and palette entry; the release test is green; no real changed-entry second copy yet (round 20)

Branch `Presentation`, pushed `cb5b55b3..a0243cde` (`git ls-remote origin Presentation` = `a0243cde2ab1f166b48eb4932df30488defc4814`). Build dir removed; unpacked captures removed (scratch r21 deleted).

Labels: CONFIRMED = a PES replay (`frame_trace.run_recorded`), a test run, a gdb backtrace, or a scan of memory; INF = inferred; OPEN = not settled.

## 1. New real captures (CONFIRMED)

copy_livein.py on each (S lines = copy taken; R+0x48 read at entry; after == palette.bin):

| capture | human / palette | R+0x48 at entry | copies | copied entry |
|---|---|---|---|---|
| pid2402670 seq19556 | 20 / 20 | 0 (first read) | yes | == palette[20] |
| pid2402670 seq19557 | 15 / 15 | nonzero (second read) | **no** (stored == palette) | — |
| pid2402670 seq20463 | 10 / 10 | 0 | yes | == palette[10] |
| pid730995 seq230 | 4 / 4 | 0 | yes | == palette[4] |
| pid730995 seq231 | 5 / 5 | 0 | yes | == palette[5] |
| pid730995 seq232 | 6 / 6 | 0 | yes | == palette[6] |
| pid730995 seq233 | 7 / 7 | 0 | yes | == palette[7] |
| pid730995 seq247 | 21 / 21 | 0 | yes | == palette[21] |

- Every copying call is a first read (R+0x48 = 0): a new record, a different palette entry from seq205's. The changed-entry *second* copy (same record, R+0x48 != 0, palette changed) is still not captured. seq19557 is the only second read in the set, and its palette is unchanged, so it does not copy.
- Copy sources: all new captures use `22be0070`/`2a7f0070` (or `2b3b0070`/`1d650110`) and differ from seq205's; the copied bytes are the palette entry, not the source.

## 2. Root cause of the red release test (CONFIRMED)

- PES copies the human's palette entry: after.bin == palette.bin for every capture (palette.bin = the entry at `unit palette + 0x80 + index*0xA0`, the unit from the manager global `0x1436F5118 -> +0x6A8`).
- The palette index is a byte read from the unit frame table at `table + frameIndex * 0x1FB0 + human * 300 + 0x36B`. `frameIndex` is the low 16-bit word of the frame key at R8 (`1402647D0`'s caller, `14217DEF0` reads it with a two-byte load at R8, the first 2-byte read in the frame; probe `rd2probe.py`).
- The fixture wrote key word 0 (so the frame index was always 0) and PresentHuman's bytes as the palette entry. Captures have different frame indices: seq205 174, flipped seq567 550, seq19556 74, seq230 24 (copied from the low word of the key at R8).

Fix (`src/pes/replay_human.{hpp,cpp}`, `tools/dev/translate/copy_livein.py`, `tests/pes_replay_human_test.cpp`):
- `ReplayFrameInputs::frameIndex` (the key's low word); `Read` writes it at the frame key (`PutU32(k, 0, in.frameIndex)`) and points the unit's frame table so the selected frame is the window (`kG2 - frameIndex * 0x1FB0`), so the byte the frame reads is the byte the fixture writes.
- copy_livein.py's I line carries the frame index; it also writes `palette.bin` (the palette entry the human selects).
- The copy tests and the release test take the call's own inputs and palette entry (`FromCapture`), not PresentHuman's.

## 3. Checks

- Release test (`AChangedEntryOnASecondReadReplacesTheCopiedEntryThroughTheRelease`, flipped seq567 state): **PASS**.
- Copy tests with seq205: 6/6 (+ ASequence, 7 total) PASS. Copy test with seq230 (palette 4): PASS. seq19556 (palette 20): PASS. (A copy test on seq247 HANGS: section 5.)
- Without any capture: 5 PASS, 3 SKIP (named reasons). Sequence + release with captures: 2 PASS.
- Native service 7/7, HUD 4/4, ledger current.
- Mutation 1 (the key word writes 0 instead of the frame index): **release test FAILS** (red). Reverted.
- Mutation 2 (unit frame table not adjusted; kG2 directly): release test **still passes** — OPEN: the table adjustment is address arithmetic the test does not observe. Reverted.
- Palette-entry candidate alone (capture's palette.bin with the key word still 0): **FAILS** (cand.log). So both the entry and the frame index are needed; the frame index is the fault that changes the palette byte.

## 4. The seq247 copy test hangs (CONFIRMED: hang; cause OPEN)

- `CopiesThePalette` with seq247 (human 21, palette 21) does not return within 120 s. A gdb interrupt shows the loop: `141E96380` (the list walk: `gpr3 = [gpr3]` until it returns to the sentinel `[X+0x30]`, calling the copy gate `1408BDF70` per node), called from `141EBF220 <- 14217DB10 <- 14217DEF0`.
- PES's walk on the same state terminates (copy_livein.py replays it to the end). So the port's list links differ from PES's: a link cycle that does not pass the sentinel, or a gate result that differs. Cause: OPEN; the next probe is the link chain in the seq247 pre-state (which pages the walk reads; `livein` covers first reads only). The release and seq205 tests do not reach this walk's divergence.

## 5. Pruning 14038E6F0 / 14038DF60 / 14038F320 (OPEN; not done)

- They are reached only from 14037FDB0 (now a boundary service). I did not finish the port-side reach: the static scan of translated direct calls from the root reached only the first translated level (the root calls 1408BDF70, 1420B6A30, 14217DE20; the release callee 14217DE20 calls 140227930, 1403FB710, 1403FB730, 141EB6FF0). A correct reach needs the port's own run (the calls the translated code makes, including the dispatch table), not the PES trace.
- How the closure handles members the port cannot reach (as it stands): `closure_destr14217.json` lists the functions the capture-bounded PES trace entered (`prove_bounded` / `snapdiff --bounded`). Members the port never reaches stay translated and never run; calls from the layer to a non-member abort loudly. Pruning them needs a port-side member list (the translated run's entries), which the proof does not write. OPEN.

## 6. Commit

- `a0243cde` on `Presentation` (pushed). Paths: `src/pes/replay_human.{hpp,cpp}`, `tests/pes_replay_human_test.cpp`, `tools/dev/translate/copy_livein.py`.
- The layer is unchanged this round (no gen run needed); the layer's `gen --check` state is the round-19 state (byte-identical at that commit).

## 7. Open items

1. seq247 copy-test hang (section 4): the list walk in `141E96380` and its pre-state.
2. Changed-entry second copy: still no real call (the rig's second reads so far had an unchanged palette; EVERY 1 captures from pid730995 are first reads).
3. Port-side member list for the closure (section 5).
4. The unit frame table adjustment (mutation 2) is not observed by any test: add a test that reads a frame at a nonzero index, or drop the adjustment if the frame table is never read elsewhere.
