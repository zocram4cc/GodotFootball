# PES port Cutover2: anime and man table readers on PES's accessors; positions and the four candidates not moved (26-10-26)

Branch `Cutover` (origin). `fb144fc3` merges origin/main `20c1a1a3` (clean). `698795e9` is the accessor switch, pushed
(`git ls-remote origin Cutover` = `698795e9`; push `2439a759..698795e9`).

Labels: CONFIRMED = read from code, a test run or a repro this pass; INF = inference; OPEN = blocker named.

## 1. Landed: the anime and man table readers run PES's accessors (CONFIRMED)

- `PesWorld::AnimeRecord` and `ManRecord` (`src/sim/onthepitch/pesworldrecord.cpp`, new) run the translated accessors
  `pes_fn_140A48910` (anime, G+0x19C8) and `pes_fn_140A48A10` (man, G+0x1DD8) on the world window (`PesMem::window`, base
  `kWorldWindowVa`): rcx = G, edx = slot. The answer (rax) is the record VA. Past the count (26) it answers entry 0, as the
  disassembly does (`CMP EDX,26; JNC` to `*(G+base)`).
- `AnimeClass`, `SetAnimeClass`, `Touching`, `SetTouching` (`pesworldrecord.hpp`) call them. Callers: `BuildRunWorld`
  (`teamAIcontroller.cpp:3453-3455`) and `ProcessPesContacts` (`match.cpp:1766`, `:1818`). Both run in switch-off mode too
  (the counts in section 2).
- Deleted: GF's table walk `PesWorld::Record` (pointer arithmetic over the bound entries). It had no other callers.
  `Entry()` stays as the write path (`SetEntry`, `BindSlot`).
- Order caveat: the walk was removed in the same edit as the agreement counter. The counter compared the translated answer
  with `Entry().record`, the data the walk read, so the comparison is the same; the walk itself was not kept alongside the
  run.

## 2. Verification (this pass)

- Red, observed: `tests/pilot/pes_world_test.cpp` `WorldRecords.AnimeAndManReadersPastTheirCountAnswerTheFirstEntry` failed
  on the old code (`array::at: __n (which is 26) >= _Nm (which is 26)`). Green after the switch. The other five cases pass
  before and after.
- Agreement, 5-min seed 7, temporary per-call counter (removed before the commit): switch-off anime 915,288 calls and man
  744,361, 0 mismatches; `--pes-loop` anime 834,372 and man 602,974, 0 mismatches.
- Repro, both modes, the merged pre-change tree and the final tree: switch-off `7ca3d08f68567016` (31521 ticks);
  `--pes-loop` `67d115b984bab251` (25398 ticks). Same as the S2 report and the 25-10-26 handover: the origin/main merge
  changes neither.
- Suites through simslot (PES_EXE, PES21_ROOT, GF_DIFF_PYTHON set): pes_world_test 6/6, pes_offball_test 73/73,
  pes_contact_test 37/37, pes_event_wiring_test 4/4, pes_team_sheet_test 5/5.
- Mutations (each built, run and reverted; the tree is back at `698795e9`, rebuilt, pes_world_test 6/6):
  - M1, `ManRecord` through the anime accessor: `WorldRecords.TouchingIsTheManRecordsByteOfItsSlot` RED. The new past-count
    case stays green: it writes and reads through the same mutated path, so it cannot tell the two tables apart.
  - M2, `AnimeRecord` at `slot + 1`: `WorldRecords.AnimeClassIsTheAnimeRecordsPlusOneByte` SIGSEGV (the shifted index names an
    unbound entry; `AnimeRecord` returns null and the byte write dereferences it). The binary stops there, so the later
    cases did not run.
- Not run: `driver.py --check` (no translated code changed; the accessors are called, not edited); the gate (Main gates).
- Ceiling (ponytail): each call builds a `PesState` (the LaneZ pattern). About 1.4-1.7 M calls per 5-min run. Timing not
  compared. A reused scratch state is the upgrade if a profile shows it.
- Tooling: `git status` in this worktree fails on the `extern/godot-cpp` symlink. Use
  `git -c status.submodule=none status --short -- . ':!extern/godot-cpp'`. Full build after the merge: 813 s; incremental
  after the header change: about 60 s.

## 3. Mover trio (0x140A6C6D0, 0x140A6C990, 0x140458740): no production caller to switch (CONFIRMED)

- The hand ports `pesmover::FloorVelocity`, `MoverVelocity` and `FloorBearingDeg` (`pesmover.{hpp,cpp}`) are called only by
  `tests/sim/pes_mover_test.cpp`.
- GF's PES-path code computes the raw velocity, and the bearing of the raw velocity, by hand. It never computes the floor.
  Sites: `BuildRunWorld` `f.vel = Flat(pl->GetMovement())` (`teamAIcontroller.cpp:3446`); heading `PesBearingDeg(0,0,mov)`
  (`pesdodge.cpp:521`, `:586`; `elizacontroller.cpp:2665`, `:2753`, `:3130`); dodge-gate inputs `gate.moveX_mps = mov.coords[0]`
  (`elizacontroller.cpp:1486-1487`, `:1521-1522`).
- The floor's input is the movement state `+0x5B0` (1 still, 5 slow walk; `pesmover.hpp:14-15`). Its writer is not decoded
  (OPEN, docs/re/pes-re-player-record.md section 2.2). `velocitystate::FloorVelocity(float)` (`elizacontroller.cpp:187`,
  `:521`) is GF's desired-speed rounding: a different quantity.
- Verdict: list for S8. Nothing switched. The three test-only ports stay until S8 decides.

## 4. 0x1404AB8E0 is not PairBearing (INF, one hop short)

- `ai/pes_fn_1404AB8E0.cpp` (37 lines) calls `pes_fn_140439E50` with rdx = man+0x55C (pos), r8 = man+0x5EC, rcx = the
  caller's rdx (the output pointer, INF), and returns that pointer.
- `ai/pes_fn_140439E50.cpp` (221 lines) takes two points, x at +0 and z at +8 each (the pos layout x +0x55C, y +0x560,
  z +0x564), subtracts, calls `pes_fn_140A92D40` (the atan2 polynomial) and folds the result into degrees. It is a bearing
  between two points.
- So 0x1404AB8E0 = bearing(record position -> the record's +0x5EC point) (INF). `PlayerFacts.aim` ("the run target, else the
  position", `pesoffball.hpp:193`) is a likely meaning of +0x5EC (INF, not decoded).
- `PairBearing` (`pesmarktable.cpp:60`) is the mark table's own `std::atan2` stand-in. It is not 0x1404AB8E0's call.
  0x140439FE0 is a table read (MA+0x228D4 rows), not an atan2.
- OPEN: name the GF hand port (a bearing from a player to his run target, if one exists in `pesoffball.cpp`) and its
  production caller, before any switch.

## 5. Positions (step 1): not moved (CONFIRMED why)

- The block's writer is known: GF movement (`Player::GetPosition`, `GetMovement`, `GetDirectionVec`).
- The PES-path readers of position are value snapshots, each its own type with its own consumers. A clean move deletes each
  copy and migrates its readers:
  - `PlayerFacts` (`pesoffball.hpp:181-193`, pos/vel/facing/aim): producer `BuildRunWorld` (`teamAIcontroller.cpp:3445-3447`,
    both modes). Consumers: `pesoffball.cpp` (146 uses of pos/vel/facing/aim/slotPoint), `pesreactionblock.{hpp,cpp}`, and
    tests `pes_offball_test.cpp` (171 uses) and `pes_reaction_block_test.cpp`.
  - Carrier snapshot (`pescarrier.hpp:1011-1015`: pos "record +0x55C/+0x564", velPerFrame, facing_deg, action).
  - Press (`posX/posZ`, docs/re/pes-re-player-record.md section 4); mark table (`pesmarktable.hpp:67-77` Facts.gateMates and
    holder; `CountGate` `pesmarktable.cpp:79-91`); dodge-gate inputs (`pesdodge.hpp:270-289`, built in
    `elizacontroller.cpp:1485-1523`).
- Not started this pass: the offball type alone is about 320 uses. The proposed first slice, for Main's OK: `PlayerFacts`
  pos/vel/facing read from the block through a `PesWorld&` the run world holds; writer = `BuildRunWorld`; the tests get one
  helper that writes the block.

## 6. Step 2 fields: writers (CONFIRMED from the translated set)

| Field | Writer | Status |
|---|---|---|
| man +0x574/+0x578/+0x57C velocity | GF movement | PES writer OPEN (tick movement); GF side known; not moved (depends on section 5) |
| man +0x5D0/+0x5D4/+0x6B8 action, prev, repeat | `0x140A6CA10` (translated, `gameplay/pes_fn_140A6CA10.cpp`): prev := action; action := edx; repeat := 0 if changed, else +1 | Writer translated. Source of the action word OPEN (the +0xB5AC copy at 0x14092D607; the 1 / 0x72 writes at 0x140A36D6F) |
| slot +0x1430/+0x1438/+0x1414/+0x1A/+0x1C/+0x22 | `0x14092ED30`: NOT translated | OPEN, left. +0x1A/+0x1C width OPEN-3 |
| man +0x5B0 movement state | no decoded writer | OPEN, left (blocks the mover floor, section 3) |
| man +0x554 facing | GF `pesDeg(GetDirectionVec())` (`teamAIcontroller.cpp:3447`) | INF: same quantity; PES writer not decoded |
| man +0x661 touching | S2 | moved earlier |

## 7. What S3 needs

1. The MI struct at `G+0x270` in the world header (S2 open item 2) before phase words can be read through the window.
2. The anime class writer (lane-units2 S12) is still OPEN; the keeper stand-in (`ponytail:`, `teamAIcontroller.cpp:3450`) stays.
3. Positions in the block (section 5) before any translated mover or bearing function can run on the block.

## 8. Open items (ranked)

1. Positions: the `PlayerFacts` consumer refactor (section 5). Needs Main's scope decision.
2. Mover trio: no production call site exists, and the floor's input (+0x5B0) has no writer. S8.
3. 0x1404AB8E0: name the GF hand port and its caller (section 4).
4. 0x140A6CA10 is translated; GF needs the PES action-word source (the +0xB5AC copy, 0x14092D607) before it can run.
5. 0x14092ED30 untranslated: the slot counters and the AI anchor stay OPEN.
6. Side tables (G+0x1928, G+0x1978) still stand null (S5/S7).
