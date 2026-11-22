# CutsceneAnchor11 (CutLibrary): the parser's output equals PES's live cut-library nodes, so the install's stadium constant file is the source; the stadium field behind the key is not found in the live match structs; the fill is not wired; the wide test is still RED.

Branch `CutsceneAnchor`, pushed head `e647d824a3f47aaf55bf2c3556549f1cfa656579` (`git ls-remote origin CutsceneAnchor`). Code this round: one correction to a committed note (results_core.json, the parser record, section 4), committed as `e647d824`. Previous commits: `4d087301` (builder), `4d33baa3` (parser). Labels: CONFIRMED = read or run this round; INF = inference; OPEN = named blocker. All live access read-only (`sudo -n python3` `os.pread` on `/proc/678718/mem`; no signal, no write, no pause). The pid is the one running now (`pgrep -x PES2021.exe`); it has a match in progress (`MI = [G+0x270] = 0x1FA04608`).

## 0. Outcome

- Live table read: the cut library holds 234 entries and the key entries are the stadium demo areas (section 1).
- Source identity CONFIRMED: PES's own parser, run emulated on the install's `demoarea_st001.o` and `demoarea_st046.o`, writes node fields equal to the live nodes of entries 0x71 and 0x9E. So the copy PES parsed is the mounted winner's `constant_stadium.bin` (section 2). The search for the object's bytes in live memory found nothing; the field comparison is the evidence instead.
- The stadium id behind the key: NOT FOUND in the live match structs (section 3). So the key formula (112 + stadium id) is still INF, and GF has nothing to feed the key from.
- The fill: not wired (the name match and entry creation are not translated; section 5). The wide test: RED, 9 EXPECT failures, unchanged.
- Correction: the record of the parser in `results_core.json` (commit `4d33baa3`) says the live node did not match. That was wrong: I read a 64-bit word in the wrong byte order (the live node has `-80.0` in the low dword of `+8`, not in the high one). The note is corrected in this round (section 4).

## 1. The live cut library (CONFIRMED, pid 678718)

- `[0x1437F4808] = 0x1F33D4A0` (the header); header `begin = 0x333B5C0`, `end = 0x333BD10`, count 234.
- Entry 0x71: entry object `0x21FF76B0` (`+8` = node `0x32894AC0`, `+0x10 = 0x100000071`); entry 0x9E: entry `0x3293C600`, node `0x329187C0`, `+0x10 = 0x10000009E`.
- Both nodes have vtable `0x142B54228` (`+0`), flag `1` at `+4`.
- Node 0x9E (stadium 46, this launch's key): `+8 = 74.094` (0x42943062), `+0xC = 6.233`, `+0x14 = -70.375`, `+0x18 = 9.7`, `+0x1C = -55.526`. Node 0x71 (stadium 1): `+8 = -80.0`, `+0xC = 16.0`, `+0x14 = 80.0`, `+0x18 = 16.0`, `+0x30 = 1.0`.
- Saved as scratch files `cutlib/live_node_71.bin` and `live_node_9e.bin` (0x158 bytes each; not committed: PES-derived).

## 2. Source identity: PES parsed the winner's file (CONFIRMED)

- PES's parser `0x141E6C280` was run emulated (`synth.py`, PES bytes from entry `0x141E6C280` to its final `ret` `0x141E6C5A7`) on the 432-byte objects `demoarea_st001.o` and `demoarea_st046.o` (unwrapped from the winner's `constant_stadium.bin`, dumped with `pesfmt_dump cat`).
- Compared with the live nodes (0x158 bytes):
  - st001 vs entry 0x71: every byte equal except offsets 0..4 (vtable, flag: written by the constructor `0x141E6C100`, not the parser).
  - st046 vs entry 0x9E: equal except offsets 0..4 (same reason) and `+0xA9..+0xAB` (live `02 3E 01`, the parser writes only the byte at `+0xA8`; the rest is heap memory from the allocator, which the constructor does not zero).
- So the live nodes are the parse of the winner's objects. The object's bytes themselves were not found in PES's memory by prefix (`livefind.py`, the first 64 bytes of st001.o): PES does not keep the unwrapped blob. The field equality is the evidence; it covers every field the loader writes.
- Node layout (CONFIRMED, live and emulated agree): for each self-relative word `w` at `rdx`, the sub-struct is at `rdx + w`; the loader copies its fields into the node at `+8..+0x58` and on. The `rdx` for the demo area is the object's own start.

## 3. The stadium field (OPEN: not found)

Tried, live, pid 678718 (match running):
- `MI = [G+0x270] = 0x1FA04608` (`mi_scan.py`, `stad_scan.py`): no `u32` or `u16` equal to 46 in `MI[0..0x1800]`; none equal to 158 either.
- The world block `G[0..0x2E58]` (`G = [0x1436F3DC8] = 0x1EBCED20`): no `u32` or `u16` equal to 46.
- The environment copy `G+0x35960+0x70` (INF per `pes-re-cutscenes.md` 0x50..0xEF, "+0 stadium id"): reads 0 (`env_scan.py`). The offset is probably in another world object; not confirmed.
- The singleton `S = [0x1437F5D50] = 0x26071880` (the match stadium source per `pes-re-stadium-cloth-vfx.md`): `S+0xF0 = 0`; no `u32` 46 in `S+0xF0 .. +0x52E0`. Also not confirmed.
- The key object `T = [G+0x2C0] = 0xABEF534` (set; it toggles to 0 about 30 times a second, `sampler.py`). Its words (`t_dump.py`): `+0x00 27`, `+0x04 50`, `+0x08 24.0`, `+0x0C 50.0`, `+0x14 0x07000014`, `+0x18 0x002E1703`, `+0x1C 0x9E`, `+0x20 0x9E`, `+0x30 4`, `+0x34 20`, `+0x38 2`, `+0x3C 33`, `+0x40 3`, `+0x44 5`, `+0x48 0.8`, `+0x4C 0.4`, `+0x58 10.0`, `+0x5C 1.0`. No 46 and no other stadium-like field. Two keys at `+0x1C` and `+0x20`.
- Not done (budget): the same fields in seq204 (launch pid2024691, key 0x71) and seq321 (launch pid678718 at worldctor, key 0x9E). The capture of seq204 is packed in `live2`; unpacking it is 3.8 GB.

Conclusion: the stadium id is not in any struct I scanned. The key-equals-112+stadium rule is INF; it is consistent with the table layout and with two launches (0x9E = st046, 0x71 = st001), but the stadium of either launch is not read yet.

## 4. The correction in the record (CONFIRMED)

- `results_core.json`, record of `0x141E6C280`: the note said the live node was not validated and that no base reproduces it. Both statements are wrong. The note now says it is validated against live entries 0x71 and 0x9E, with the two listed differences (section 2). Its numbers (1 call, 0 mismatches, own-block 12/13) are unchanged.
- The record's commit message (`4d33baa3`) carries the same wrong sentence; the commit message cannot be changed without a rewrite, so this note is the correction.

## 5. Wiring the fill (not done; the blocker is the translation, not the bytes)

- The bytes now match live for the parser, so the bytes are no longer a blocker.
- Still not translated: the name match `0x141E5CB40`, the entry init `0x141E5E0A0 / 0x141E5E210`, the factory and constructor for demo areas `0x141E6C240 / 0x141E6C100` (the 0x158 alloc is the allocator native service), and the caller that builds the object list (`0x141E5D560`, reached through vtable `0x142B48C88`, whose installer is still not found in the image). Without the caller, GF has no list to pass.
- The other 233 record factories are not decoded (84 `0x141E6C240`, 34 `0x141E5F430`, 21 `0x141E621B0`, 15 `0x141E7C240`, and singles).
- Wiring one entry (0x71) without the list and the name match would be a stand-in: rejected by the rules.
- Boundary (approved earlier, still the plan): native `PesVfs::Read("common/match/constant/constant_stadium.bin")` → `UnwrapWesys` → WESYS object table; translated above it.

## 6. Key writer and hook (OPEN)

- The writer of `T+0x1C` (and the second copy at `+0x20`) is not found. Static candidates from `wscan.out` / `w3scan.out` are unchanged (section in CutsceneAnchor10).
- The sampler shows the key is constant (0x9E) for the whole run so far: no goal yet.
- No hook line supplied: the writer candidate is still not named. Arming a hook on one of the 191 `+0x2C0` writers would be a guess.

## 7. Checks

- This round changed no GF code. `gen_translated.py --check` and `ledger.py --check` were run after the note correction (results below in the final report message).
- No tests were rerun for this round (no code change); the last run (CutsceneAnchor10): `pes_fixdemo_select_test` 7/7, `pes_registry_test` 7/7, `pes_goal_anchor_test` non-wide 22/22, wide RED (9 failures).

## 8. Open items (numbered)

1. The stadium id of each launch: read `Team.bin`'s home stadium for the two matches (u16 at `+0x3C`), or find the PES field holding the match's stadium (scan the seq204 and seq321 captures' world and match-info blocks for 46 and 1 at the same offset). Blocks the key formula.
2. The writer of `T+0x1C` and `T+0x20`: a goal sampled in a live match (the sampler is ready: `cutlib/sampler.py PID SECONDS`); then a hook on the writer.
3. Translate the name match `0x141E5CB40` and the entry init `0x141E5E0A0 / 0x141E5E210`, plus the demo-area factory `0x141E6C240` and constructor `0x141E6C100` (the allocator is native), under bounded proofs with the list object from the caller.
4. Find the installer of vtable `0x142B48C88` (the fill's caller) and its list source.
5. Decode the other 233 record factories and the loader slot `+0x10` of `0x142B502C8` (`0x15B35E0C0`).
6. Wire the fill (native read + translated parse) and feed the key from GF's own stadium choice through the PES field found in item 1; then the wide test; then T.x = -41.6211472 (INF, not verified).
