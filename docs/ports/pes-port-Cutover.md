# PES port Cutover: S1 done for the tables GF reads, S2 partial (block, two fields moved)

Branch `Cutover` (origin, head `2439a759`). Commits: `e6e6b21e` red test (observed red: `onthepitch/pesworldrecord.hpp`
missing); `aec3b593` S1 and S2 (partial); `2439a759` anime class keyed by PES slot (the final code).

Labels: CONFIRMED = read from translated code, docs or a test run this pass; INF = inference; OPEN = blocker named.

## 1. S1: G's record tables as pointer windows

- `src/sim/onthepitch/pesworldrecord.hpp`: G's five record tables at PES offsets, 40-byte entries whose first qword
  is the record VA, G header 0x2E58, one window at `kWorldWindowVa` = 0x2000100000 (clear of the team sheet window at
  0x2000000000). Accessor VAs in comments. CONFIRMED from the translated bodies: `cmp r32,count; jnc` falls back to
  `*(G+base)`; the entry address is `G + base + 0x28*i`.

| Table | G offset | Count | Accessor | Entry names |
|---|---|---|---|---|
| side A | 0x1928 | 2 | 0x140A48A30 | team pointer (INF, docs/re/pes-re-press-eval.md) |
| side B | 0x1978 | 2 | 0x140A489D0 | team pointer (INF) |
| anime | 0x19C8 | 26 | 0x140A48910 | anime record (block +0x0) |
| man | 0x1DD8 | 26 | 0x140A48A10 | man record (block +0x32C) |
| slot | 0x25F8 | 22 | 0x140A489F0 | slot record (block +0x10F0) |

- Static asserts on every table offset, the entry size and the header size.
- GF readers of the tables (production): the man entry (touching flag) and the anime entry (class byte), through
  `PesWorld::Record`. The slot and side tables have no GF reader yet. Side entries stay null (ponytail: they name
  team records, which join the window with S5/S7).
- Deleted: none. GF holds no G, so there was no pointer chasing to remove.

## 2. S2: the per-player block (partial)

- Block stride 0x3F40 per on-pitch slot: anime +0x0, man +0x32C, slot +0x10F0 (docs/re/pes-re-player-record.md 1.2).
  Field offsets named in the header, with bounds asserted (man ends before the slot record; slot fields end inside the block).
- Slot key: `Match::PesSlotOf` = side * 11 + squad place (the place the event store keys use). Substitutes swap into the
  leaver's place (team.cpp), so on-pitch players stay inside 0..10.
- **Moved, old copy deleted:** man +0x661 `touching`. `PesContactState::touching` is gone. Its writer and four
  readers in `Match::ProcessPesContacts` go through the man table. Its value is written each tick before any read,
  so the move changes no behaviour (repro below).
- **Moved, stand-in kept as the writer:** anime +0x1 class byte. `BuildRunWorld` writes the keeper stand-in
  (`roleClass == kKeeperClass ? 3 : 0`) to the player's anime record (keyed by PES slot) and reads it back into
  `recordByte1`. ponytail: PES's writer of +0x1 (lane-units2 S12) is not decoded.
- **Not moved (listed in the plan):** man +0x55C/+0x564 (position), +0x574/+0x578/+0x57C (velocity), +0x5D0/+0x5D4
  (action), +0x6B8 (repeat); slot +0x1414 (anchor), +0x1430/+0x1438 (counters), +0x1A/+0x1C/+0x22 (u16 triple).
  GF keeps position, velocity, facing and action per call in snapshot structs (pescarrier, pesmover, pespress,
  pesmarktable, pesreactionblock, peslane, pesdodge, pesscorerows) and has no per-tick PES-layout writer. The
  counters' writer FUN_14092ED30 (RL-06) is untranslated. Moving these needs the writers first.
- **Translated functions switched over in production: none.** Candidates and why they wait: 0x140A6C6D0 (floored
  velocity), 0x140A6C990 (mover velocity), 0x140458740 (heading), 0x1404AB8E0 (pair bearing, hand port `PairBearing`).
  Each reads man fields that have no per-tick writer yet. The accessors 0x140A48910/A48A10/A489F0 are proven (section 4)
  but GF's table readers still index the entries itself.

## 3. Repro and invariance (seed 7, 5 min, `gfsim_run --minutes 5`)

| Build | switch-off (GF mode) | `--pes-loop` | ticks (off / pes-loop) |
|---|---|---|---|
| main (origin/main 26577b17, pre-change binary) | `7ca3d08f68567016` | `67d115b984bab251` | 31521 / 25398 |
| cut1 (`aec3b593`) | `7ca3d08f68567016` | `67d115b984bab251` | 31521 / 25398 |
| cut2 (`2439a759`, final code) | `7ca3d08f68567016` | `67d115b984bab251` | 31521 / 25398 |

Equal in both modes. The two hashes match the 25-10-26 handover. Logs: `<workdrive>/gf-Cutover-scratch/repro_*.log`.
Only the wall-clock `[profile-split]` timings differ between runs.

## 4. Proofs

- `tests/pilot/pes_world_test.cpp`, 5 cases, 5/5 PASSED on the final binary (`test_final.log`):
  - `EachAccessorReturnsTheEntryItsIndexNames`: each of the five translated accessors returns the entry its index
    names, for every in-range index of its table.
  - `AccessorsFallBackToTheFirstEntryPastTheirCount`: index = count answers with entry 0.
  - `FieldsSitAtTheirPesOffsetsFromTheRecordTheAccessorReturns`: anime +0x1, man +0x55C and slot +0x1430 read through
    the accessor's pointer are the bytes GF wrote.
  - `TouchingIsTheManRecordsByteOfItsSlot`, `AnimeClassIsTheAnimeRecordsPlusOneByte`: the GF accessors.
- Touched suites, run with `PES_EXE`, `PES21_ROOT` and `GF_DIFF_PYTHON` set, through `simslot` (`touched_suites2.log`):
  `pes_event_wiring_test` 4/4 (builds a Match, so the world is created and bound), `pes_offball_test` 73/73,
  `pes_contact_test` 37/37, `pes_team_sheet_test` 5/5. A first run without the PES variables skipped some cases;
  its counts are superseded.
- **Mutations (each built, run, reverted):**
  - M1 (S1, `BindSlot`: man entry set to the block start, dropping +0x32C): `FieldsSitAtTheirPesOffsetsFromTheRecordTheAccessorReturns`
    RED (pointer 812 bytes = 0x32C short of `ManVa`); other four green. Caught.
  - M2 (S2, `SetTouching` writes anime +0x1 instead of man +0x661): `TouchingIsTheManRecordsByteOfItsSlot` RED
    (`Touching` false, accessor reads 0 at +0x661); other four green. Caught.
  - Both reverted; the tree is back at `2439a759` (`git status` clean for src, tests, tools, docs) and the build is clean.
- Owner ruling (26-10-26), coverage reported, not gated. Accessors: 5 of 5 proven on every case run (return value).
  Coverage: both branches of each accessor (in range, fallback), on synthetic GF worlds with every table index.
  Low coverage: the side tables (2 entries each, not bound in GF). Not yet done: real-call cases from a captured match
  (synthetic worlds only), and a written-bytes check (the accessors write no memory, not asserted by a test).
- `driver.py --check` on the recorded sets: not run. No translated code changed; Main re-measures under NoFloor.
- Gate (`tools/dev/gate.sh`): not run (Main gates).

## 5. Open items (numbered; S3 needs 1 to 3 first)

1. **The side tables name team records, and PesMem has one window.** T (PesTeamRecord) must join the world window
   (S5), or PesMem gains a window list. Until then G+0x1928/0x1978 stay null.
2. **MI for S3.** MI = `*(G+0x270)` (docs/re/pes-re-player-record.md, MA/MI notes). The world header needs the MI struct
   at that offset before S3's phase words can be read through the window.
3. **Per-tick writers for block fields.** Position, velocity, facing and action must be written from GF movement
   each tick before any PES-path reader can use them (S2 remainder). Until then the fields are zero.
4. **Slot fields without a writer.** +0x1414, +0x1430, +0x1438, +0x1A/+0x1C/+0x22: writer FUN_14092ED30 (RL-06) is
   untranslated; +0x1A/+0x1C width OPEN-3.
5. **Anime class writer** (lane-units2 S12) OPEN; the keeper stand-in stays until decoded.
6. **Block allocator** (OPEN-1): PES's allocation is not found; the match-owned array stands in.
7. **Production switch-over of the accessors.** GF's readers index the entries itself. The LaneZ pattern would call
   0x140A48910/A48A10 instead; deferred until the proof covers captured calls.
8. **Real-call proof.** Capture a snapshot of a running match and prove the accessors and the block reads on live
   memory (owner ruling: real-call cases preferred).
