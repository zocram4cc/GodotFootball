# pes-port-PresReplay15: the copy branch reads a field A0DE0 writes from the palette source; the port never stores that source (round 14)

Branch `Presentation` unchanged: last pushed `2e76b84c` (`git ls-remote origin Presentation` = `2e76b84c5fae`). No code change this round (investigation; nothing to commit). Copy tests still red. Build dir removed; decompressed captures removed; scratch 29 MB.

Labels: CONFIRMED = a unicorn replay of PES's own bytes on a real capture (`live2/pid2024691/seq205`, the only capture in the set that performs the copy), a gdb read of the port, or a layer line. INF = inferred.

## 1. Which captures cover the copy (CONFIRMED)

- Swept the replay-frame captures with PES's bytes (`trace_r14.py`): `pid2998104` seq567, seq960, seq1464, seq1954, seq1955 and `pid2695300` seq015 perform no create (one frame event, no `E6B0`). This agrees with the presentation record's note: the real captures have `R+0x48 != 0` and skip the copy; the synthetic states (flipped `R+0x50`) are the ones that copy.
- `pid2024691/seq205` is the one real capture that copies: 308 `E6B0`/`E3E0` calls, three `A2090`, three `A0DE0`, 17 calls of `14239EFD0`, and **zero** calls of `14239EF20`. So PES never takes the `14239EF20` branch on a real capture; the port's `OUT-OF-SET` gap is not PES behaviour (answers item 2 of the ruling: no proof record for `14239EF20` is needed on this evidence).

## 2. r14 is the carved object, and the carved block is not zero-filled (CONFIRMED)

- The `A0DE0` argument is `rcx` (`gpr1`), copied into `gpr14` at line 56 of the layer body. The copy path reaches it as `A2090(rcx = [O] from D370)`, and `D370` returns `block + 0x10` from the `E6B0` carve (e.g. `E6B0` returns block `0x2bbe2b00`; `A0DE0` gets object `0x2bbe2b10`).
- The carve contents (the `E3E0` return dump, `seq205`): the header is `0x10, 0x0, 0x4000d00, 0x178, 0xdead, ...`; the payload is **not zero**: block `0x2bbe2b00` at return has 20 non-zero qwords of 32 in its first 0x100 bytes, and older carves hold previous contents (`0xDEAD` markers, float data, `0x2bbe..` pointers). A block carved from fresh memory (`0x1f6ac900`) is all zero. So `E3E0` leaves the payload as the region's previous contents, not a fill.
- But the branch does not read the carved payload. `A0DE0` itself writes the object (CONFIRMED, pcs `0x1423a0e8b..0x1423a0ed9`): it zeroes `O+0x40 .. O+0xC8` (its own stores of 0), then `O+0xE8` (`= gpr12`) is exchanged with the palette source pointer, then `O+0x60` is set from the source (`[src+0x60]` when `[src+0x40] dword != 2`, else `gpr3`). The test `[O+0x60] != 0` is therefore a test of the palette source's `+0x60`, and the carve fill is overwritten before the test. **The 0xFF-fill hypothesis is refuted on this path**, not merely masked.

## 3. Where the port diverges (CONFIRMED)

- PES's real run: `O+0xE8` is stored with the source pointer `0x22e50570` at trace index 251 by `1403DB290` (an exchange: `t = [rcx]; [rcx] = rdx; rdx = t`) with return address `0x1402647F9`, i.e. the caller is `pes_fn_1402647D0` (in the layer). Then `O+0x60 = 0x1f760400` (`[0x22e50570 + 0x60]`), non-zero, so PES takes the `EFD0` branch.
- Port (gdb at `pes_fn_1423A0DE0.cpp:178`, the `gpr0 = rd(gpr12)` line): `r14 = 0x2400001c00` (= object), `r12 = 0x2400001ce8` (= object + 0xE8, consistent), **`[r12] = 0`**. The port's `1402647D0` did not store the source pointer before `A0DE0`, so `[O+0x60]` comes out zero and the port takes the `OUT-OF-SET` branch.
- OPEN: why the port's `1402647D0` does not store a source. Candidate causes, to be decided by the next trace: (a) the port computes a different source (a GF palette address instead of PES's heap entry) and the value is zero, i.e. the palette lookup in `1402647D0`'s caller returns 0; (b) the caller of `1402647D0` takes a different branch in the port (the copy gate `1408BDF70` path). The next instrument: gdb at `pes_fn_1402647D0` entry and at its `DB290` call, printing the source it passes and the caller's branch inputs, against PES's values from the unicorn trace of `seq205` (the source `0x22e50570` = the palette entry, `+0x60 = 0x1f760400`).

## 4. Owner's items

1. **r14 origin:** done (section 2). **Carve fill:** `E3E0` does not zero-fill (CONFIRMED from real carves); but the branch is decided by the palette source written by `A0DE0`/`1402647D0`, not by the carve, so the carve fill is not the blocker. The `0xFF` test was not "masked" by other state: the field it would affect is overwritten before the test on the real path.
2. **14239EF20 proof record:** not needed on the real capture (PES never reaches it there). The gap is the port's missing source store (section 3). If a later trace shows PES does reach `14239EF20` on some other capture, the record would come from `seq205` with the proof tool (`prove_bounded.py --real`), since `seq205` is the real copy capture.
3. **Regenerate closures, delete dead import models:** NOT done. The region-manager members (`F230`, `F320`, `AB660`, `AD8A0`, `AC320`, `AC460`, `AC600`, `EE80`) are members of the presentation record's closure file `closure_destr14217.json` (356 members). Regenerating it needs a snapdiff proof run of `14217DEF0` with the real `seq205` capture and the synthetic states; that run has not been done. The import models stay until the closure is regenerated (they are reached only through those members).
4. **CMake registration:** verified. `tests/CMakeLists.txt` has one registration (three lines: `add_executable`, `target_link_libraries`, `gtest_discover_tests`), no duplicate. No redo needed.

## 5. Ruling needed / next step

- Next instrument (one gdb run + one unicorn trace): the `1402647D0` source value and its caller branch on the real capture vs the port. If the port's source is zero because of the palette lookup, the palette entry the port passes must match `0x22e50570`'s role (a palette entry with `+0x60` non-zero); the test's synthetic palette entry (`in.entry[i] = i*7+3`) is the candidate to check.
- The copy tests stay red and are not skipped.
