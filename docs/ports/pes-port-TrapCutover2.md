# pes-port-TrapCutover2: the slot record's checked stamina cells are written in PES's layout; the match no longer aborts in the trap chain; the trap family is every Animetable motion (30-10-26)

Branch `StoppageFlow`, head `6b331481` (`git ls-remote origin StoppageFlow` confirms; earlier pushes `dffb963d` merge of `origin/main`, `60a91e98` stamina cells, `db878d63` clips + closure). Continues `docs/ports/pes-port-TrapCutover.md` section 5. Tags: **CONFIRMED** = a tool run this session; **INF** = inference; **OPEN** = not settled (blocker named). Image `pes_full.bin`, base `0x140000000`.

## 0. Verdict against the acceptance

| Acceptance | State |
|---|---|
| 1. `git merge origin/main`, push | DONE (`dffb963d`) |
| 2. Stamina cells written in PES's layout, equal to PES's on the full captures | DONE, CONFIRMED: `pes_stamina_cells_test` 4/4, 44 captured players x 3 cells; red-first (the abort); 3 mutations run |
| 3. Trap family = the clips the chain can commit, `trapRows` deleted | DONE (trap family = every motion an Animetable cell names; `trapRows`, `ActionRow`, the picker-row tests deleted). The action's own tick (S5) is **not** ported: named stand-in (section 3) |
| 4. `pes_stall_test` live-ball case green | **NOT GREEN: 140 firings (baseline 147).** The abort is gone and the match finishes at full time; the remaining firings are a different cause (section 4) |
| 4. Smokes, determinism lines | DONE: both paths, two runs each, equal hashes, rc 0 (section 5) |
| 4. Fast-tier interim line | `GATE fast (interim): pes_stamina_cells 4/4, pes_trap_oracle 8/8, pes_receive 48/48, pes_clips 12/12, pes_stall armed 1/1 and live FAIL (140 firings); gen_translated --check rc 0 (4223 functions byte-identical)`. `GATE_TIER=fast tools/dev/gate.sh` was not run (the tier arrived after this work began; load ~30) |
| 5. `PES_EXE` documented | DONE: `tools/pes21_import/README.md` ("A match needs the image"), `README.md`, `AGENTS.md` |

## 1. The checked integer (CONFIRMED, decompile + capture)

Reader `0x140A58550` (tank percent `cur*100/max`), `0x140A581C0` (reserve percent), getters `0x140A58350` (cur), `0x140A58450` (max), `0x140A580B0` (breath). Setters: **`0x140A595C0` max** (hash `+0xE8`, key `+0xF0`, index `+0xF4`, ring `+0xF8`), **`0x140A592C0` cur** (cells at `+0xC8`; then, if `max < cur`, the max cell is copied over the cur cell, and slot byte `+0x61 = 1`), **`0x140A58F60` breath** (`+0x108`; then the tank percent is read back, `cap = max(pct, 0x14) * 0x780 / 100`, and a breath above the cap is stored again as the cap). Cell layout (0x20 bytes): `u64 hash @+0` = FNV-1a-64 over the value's four bytes LSB first; `u32 key @+8`; `u32 index @+0xC`; `u32 ring[4] @+0x10`. Store: `ring[index & 3] = encode(value, key)` with `e_i = (v_i ^ (K_i * 0x3F)) - K_i`, `K_i` = byte i of `(key * 3 + 4 * i)`; `index` = FNV-1a-32 of the clock dword `_DAT_7ffe0008` (KUSER_SHARED_DATA+8). The setters never change the key. The decoded layout equals `trap_oracle.py checked_int` and `pes-re-stamina-drain.md` section 1.

Key source (INF, not reproduced): the initializers `0x140A57B00` / `0x140A3B640` set `key = (u32) FNV-1a-64(cell address)`, but the 132 captured cells' keys are not that hash of their addresses (script, this session): cur and max share a key (the clamp copies a whole cell), the breath cell has its own. So the record is built elsewhere and moved, or the key is rewritten. It decodes to the same value under any key.

## 2. What landed

- `pesattrs.{hpp,cpp}`: `StoreGuardedWord(set, word, index)` (the store the three setters share, the inverse of the existing `ReadGuardedWord`) and `GuardedIndexOf(clockDword)`.
- `pesstamina.{hpp,cpp}`: `kSlotStaminaCurOffset/MaxOffset/BreathOffset/DirtyOffset`; `WriteSlotStamina(record, constants, state, clockDword)`: max, cur (clamp + dirty byte), breath (cap clamp) in PES's order. The cap is the existing `BreathCap` (image constants).
- `pesworldrecord.hpp`: `PesWorld::SlotRecord(slot)`. `WritePesTrapActors` (`teamAIcontroller.cpp`) writes the three cells for every active man each tick from `Player::GetStamina()`.
- `tools/dev/translate/stamina_scan.py` (physique_scan style: per match::Player of a full capture, the three cells raw, TSV).
- `tests/pes_stamina_cells_test.cpp` (4 tests; `PES21_ROOT`, `GF_PES_STAMINA_SCAN`):
  1. `PesOwnReadersAnswerTheStatesPercents`: the translated `0x140A58550` / `0x140A581C0` run on a GF-written record (keyed cells, three tank sizes x six cur x six breath, incl. breath above the cap and no tank) answer what the state says;
  2. `TheSettersClampLikePes`: cur above max becomes the max cell, breath above the cap becomes the cap, dirty byte set;
  3. `ACorruptedCellAbortsPesReader` (`EXPECT_DEATH`): a flipped ring word aborts PES's reader, so the passing runs mean something;
  4. `EveryScannedPlayersCellsEqualGfs`: for every captured player and cell, GF's store of PES's decoded value under PES's key and index equals PES's cell (hash, key, index, the selected ring word), PES's states never need the clamps, and PES's readers answer alike on GF's record and PES's record. Scan over `live2/ticks_pid91171/snap1-224647`, `snap3-224751`: 44 players, 132 cells, all equal.
- **Translated layer.** The stall run's first abort after the cells was `PES_DISPATCH_UNTRANSLATED: 0x140851E40`, a virtual method of a seven-class family (vtables `0x1425EAC18`, `0x1425EACC0`, `0x1425F1A30/AD8/B80/C28/CD0`, slot 8) that `closure_trapchain.json` (the methods the 7 captures reached) lacked: the committed trap chain was not self-contained for in-match calls. `closure.py --tag trapchain2` over the 15 untranslated methods of those vtables (97 members with their direct callees, 43 new universe rows, 97 specs under `diffharness/specs/core`, set file `closure-trapchain2.txt`), the 23 VAs not yet members appended to `closure_trapchain.json`, `gen_translated.py` regenerated: 4223 functions (537 closure, +22 files). `gen_translated.py --check` rc 0. These closure members are not recorded bit-exact on their own (the harness has no corpus for them); they run only in a match.
- **Clips.** `LoadPesClips` loads, as family `kTrap`, every motion any Animetable cell names (`LoadAnimetable`; a motion missing from the motion table or without a body clip fails the load). The five `gkcatch_*` rows are gone. `_TrapCommand` no longer checks `HasSpecial`. Deleted: `ActionRow`, `kTrapActionRow`, `ReceiveConstants::trapRows`, the action-clip table loader, `TheTrapRowsAreTheInstallsActionSevenWindows`, `ALoosePresserInTheTrapWindowPlaysThatRowsClip` (its premise, the picker's rows, is gone). `TheTrapFamilyHoldsTheClipsTheTrapRowsName` became `TheTrapFamilyHoldsEveryMotionAnAnimetableCellNames` (every cell motion loaded once, nothing else). `TheStandbyMotionsAreInTheLocomotionSpecialSet` now picks the locomotion copy (a motion can be in both families, loaded twice).
- Docs: `tools/pes21_import/README.md` section "A match needs the image", `README.md`, `AGENTS.md`.

## 3. Named stand-ins (each `ponytail:` in code)

1. `WriteSlotStamina`: cell keys stay as the record holds them (0 in a fresh window); the ring-index clock dword is the match clock (`GetActualTime_ms`): no GF source for either; no PES reader's result depends on them.
2. `_TrapCommand`: PES plays the committed plan from the Trap action's own tick (`FUN_140797CD0`: hit frame `FUN_1406B03D0`, duration `FUN_1406AF620`, root solver `FUN_1406E85B0`, motion issue `FUN_1406F02F0`, S5 of `pes-re-trap-inputs.md`). GF issues the committed motion at once through the Special command, the touch frame being the clip's first HitData record (`AddPesClips`), the mirror choice and root solve GF's. S5 was not ported: no capture of the tick exists (`trap_action_tick_797CD0` hook H1 is not in the watcher allowlist), so it has no oracle. A clip whose Animetable cell carries the mirror flag (`0x2000`) is played unmirrored by GF's choice (OPEN: where the plan keeps the flag).
3. A refused trap queues no command (see 4).
4. Carried over from TrapCutover section 3 (action ids, `P+0x2FC` ring, SBn, kind 4): unchanged.

## 4. Why the live-ball case is still red (OPEN)

- Baseline (before this work, from TrapOpen2/TrapClipPort): 147 live firings. After the cells and the translated closure the match runs to full time with **140** (this branch, seed 7, 2 min). The census counts ticks on which the stalemate breaker fired with the ball live. The `[stalemate]` lines of every run show the same case: `inPlay 1 setPiece 0 | buf active 0 prepared 1 started 1 piece 5 (or 1) taker H2/A9 start-in -1.0s`, ball at 0.08 m/s, and the **restart taker** (not a receiving man) cycling through movement, `avoidjump_*`, `idle_*_push_aside` and trip clips instead of taking the kick. That is the restart flow (this branch's namesake), not the first touch; it is the same with the old picker.
- Experiment: letting a refused trap fall through to the later entries of the reaction walk (PES's `FUN_1409F8220` returns 0 on a refusal, CONFIRMED in the decompile at `0x1409F8538`; "the walk continues" is INF from the walk's description) moved the live firings from **140 to 1226** ticks (the taker took the dodge entries). Reverted; the finding stays in the `ponytail:` comment in `_TrapCommand`. The cause to chase is why the released taker's reaction walk reaches a dodge entry at all (the taker should be in the restart's own action): `ElizaController::_ReactionCommands` is run for him.
- The armed-restart case passes (0 firings), the match finishes (no abort).

## 5. Verification run

- Red first: `pes_stall_test` aborted in the first match on the merged tree (`pes_fn_140A58550` cold path), then, with the cells, in `PES_DISPATCH_UNTRANSLATED: 0x140851e40`; both gone. The cells test did not exist on `origin/main` (does not link).
- **Mutations (each run, rebuilt, reverted):** (M1) `StoreGuardedWord` ring encode without `- key`: `pes_stamina_cells_test` aborts in PES's own reader (and `EveryScannedPlayersCellsEqualGfs` fails with unkeyed records, which is why the records are keyed); (M2) `WriteSlotStamina`'s cur clamp disabled: `TheSettersClampLikePes` fails (4 expectations); (M3) the breath cap clamp disabled: `PesOwnReadersAnswerTheStatesPercents` fails. Not mutated: the dirty-byte store, the index function (covered by the capture test only as far as the index is passed in), `WritePesTrapActors`'s call (its absence is the abort above).
- Tests run: `pes_stamina_cells_test` 4/4; `pes_trap_oracle_test` 8/8 (24 oracle dirs, `PES_EXE`, `PES21_ROOT`); `pes_receive_test` 48/48; `pes_clips_test` 12/12 (11/12 before the standby-test fix); `pes_stall_test` armed OK, live FAIL (140).
- `gen_translated.py --check`: rc 0, 4223 functions, 3 held out as before (`0x1403BB670`, `0x1405549C0`, `0x14202BA30`).
- Smoke `gfsim_run --seed 7 --minutes 2` (this branch, sim slots 2-3), each path twice: GF path `[determinism] 11c83698afc83d13` both runs (passes 13-10, shots 0-6); PES path (`--pes-loop`) `[determinism] 29fe1ad4537b09a5` both runs (passes 17-24, shots 0-0, goals 0-1), rc 0 everywhere, no abort. Not compared with main's line (no main run this session). The PES path's shots 0-0 is a tripwire reading, not a target.
- Not run: `GATE_TIER=fast tools/dev/gate.sh`, the other `pes_*` binaries that include `pesworldrecord.hpp`/`pesreceive.hpp` (only the ones above were built and run). A `#deps 0` check after the `pesworldrecord.hpp` change (an accessor only, no layout change): not needed.

## 6. Open items

1. The restart taker's stall (section 4): trace the released taker's reaction walk (`_ReactionCommands`) in the two-minute seed-7 match; then re-run the fall-through experiment.
2. S5 (`FUN_140797CD0`): needs hook H1 (`trap_action_tick_797CD0`, FIRST 1 EVERY large, snapshot when `P+0xAC8 == 7`) in the watcher allowlist for an oracle. Main: say if you want it.
3. Cell keys: where PES derives the slot record's keys (`0x140A57B00` assigns `FNV64(cell address)`; the captures differ).
4. The Animetable mirror flag in the committed plan (stand-in 2).
5. `pesclips` loads a motion twice when it is in both the locomotion and trap families (load time/memory); a shared parse would remove it.
6. `closure-trapchain2` members were generated under spec set `core`; they have no recorded corpus (they run only in a match). The stall smoke exercised them; a trap-chain replay capture that reaches the dodge-evaluator family would prove them.
