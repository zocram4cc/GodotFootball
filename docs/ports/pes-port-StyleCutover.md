# StyleCutover: the run-depth site follows PES's rule on the style word; GF's per-style depth table removed (StyleCutover, 01-11-26)

Branch `DribbleProposal` (worktree `gf-DribbleProposal`), pushed: `e5002c16` (`git ls-remote` confirmed). Continues `pes-port-DribbleRow.md` Left 4. CONFIRMED = decompile (bridge), test run or smoke run this session; INF = inference; OPEN = not settled. The image is `pes_full.bin`; the bridge decompile of `FUN_1405D8B70` is in scratch (not committed).

## 0. Verdict

| Step | State |
|---|---|
| 1 `git merge origin/main`, push | DONE: the branch was behind main `83011733`; the merge fast-forwarded to it, pushed. |
| 2 PES's consumer of the style word for the run depth | `FUN_1405D8B70` L650-657, via `FUN_140A64030(slot, bit, ...)` -> `FUN_1408CB0B0` (below). Already ported as `pes::offball::StyleRunDepth` (StylePort2); the site at `teamAIcontroller.cpp:869` used it only under pes_loop and only for the GF enum values GoalPoacher/FoxInTheBox. Now: both loop paths, fed by `Match::StyleWordOf` (the save row's +0x48, else the enum's word), no enum test. `PlayingStyles::GetDepthOffset` deleted (header, source; no other caller, grep of `src/` and `tests/`). |
| 3 red-first tests, mutations | DONE (section 3). |
| 4 smokes | Both moved; explained by two ablations (section 4). |

## 1. The PES rule (CONFIRMED, decompile of `0x1405D8B70`)

- `FUN_1408CB0B0(rec, bit, row)` (decompile): `(*(u32*)(rec+0x48) & (1<<bit)) != 0 && (*(u32*)(0x142613D20 + row*4) & (1<<bit)) != 0`. `rec+0x48` is the style word (DribbleRow §1); `FUN_140A64030` fetches `rec` and the class row (`+0x4C00[slot%11]`).
- In `FUN_1405D8B70`, per candidate man: not the ball owner (`uVar12 != local_1cc`), his action not in the run set (`FUN_1405A4790(...,0,1)` = 0), kind `uVar33` in 5..9 (kinds 5-6 need `local_1d6 = team+0x7B84 < 2`, OPEN, undecoded: kept as the existing `ponytail:`; kinds 7-9 always). Then, as three separate statements (decompile L650-662):
  1. `FUN_140A64030(uVar12, 0, ...)` true: `fVar36 += s * DAT_14259222C` (5.0, `styleRunDepth_m`, read from the install);
  2. `FUN_140A64030(uVar12, 2, ...)` true: `fVar36 += s * DAT_14259222C`;
  3. `FUN_140A64030(uVar12, 4, ...)` true: `fVar36 -= s * fVar30` (`fVar30 = s * team[+0x3A9C + slot*12]`, a per-slot team-record x, not a constant).
  `s` = `*(char*)(rec+0x254)`, the attack sign.
- Correction to the earlier port: the two additions are NOT either-or. `StyleRunDepth` returned `poacher || fox ? 5 : 0`; it is now additive per bit (a word holding bits 0 and 2 gets 10 m). A save row holds one style so the real word has one bit; the fix is faithfulness, and the unit test pins it.
- Bit 4 (Creative Playmaker) is not ported: its term subtracts a team-record coordinate GF does not carry (`ponytail:` in `StyleRunDepth` names `team+0x3A9C`). It is a style-word consumer, so it will land with that record.
- The other 16 styles have no depth term in this function (`pes-re-style-effects.md` §GetDepthOffset already said so); GF's table for them (`GetDepthOffset`: Dummy Runner +3, Hole +5, Box to Box ±4, Anchor −5/−2, ...) had no PES site and is deleted, not kept as a stand-in.

## 2. Changes

- `teamAIcontroller.cpp` `GetAdaptedFormationPosition`: `styleDepth = StyleRunDepth(constants, RoleClassOfSlot(FormationSlotOf(player)), match->StyleWordOf(data), GetRunState(player), designated == player)`, no `PesLoop()` gate, no enum. The local `style` (enum) stays for `GetWidthOffset` and the poacher's offside-line spot only, which are other GF stand-ins (`ponytail:` naming `FUN_1405E6E80` L527-582; the poacher spot has no PES site).
- `pesoffball.cpp` `StyleRunDepth`: additive per bit (bits 0 and 2), comment on bit 4.
- `playingstyles.hpp/.cpp`: `GetDepthOffset` removed.
- Dead enum uses: none left in the depth path; `GetWidthOffset`, `GetPoacherTargetX`, `GetDribble*` still use the enum (Left).

## 3. Tests (run directly, no sim slot, `PES21_ROOT=<workdrive>/PES21`)

- `pes_playerdb_wiring_test` **10/10** (6 old + 4 new `StyleRunDepthSite.*`, controller level: real fixture match, home CF with a PES id and a hand-built save row; one `Process()` first because the controller's possession/tactic members are unset before its first tick: without it the x differed run to run by 8 m, found in a first red run and fixed).
  - `TwoSavedStyleWordsThatDifferGiveTheForwardTheTermOrNot`: Goal Poacher word (bit 0) vs Hole Player word (bit 9): the spot moves by exactly the install's `styleRunDepth_m` toward the opponent's goal, both loop paths; the compat row is asserted open first (read from the install, no pinned number).
  - `FoxInTheBoxWordTakesTheSameTermAsThePoacher`, `TheDatabaseStyleEnumNoLongerMovesTheSpotWhenTheSavedWordIsNone` (database style Box to Box, word none: same spot as no style), `APlayerWithoutARowTakesTheTermFromHisDatabaseStylesWord` (no row: `StyleWordOf` falls back to the enum's word).
- `pes_offball_test` **77/77** (`OffBallStyleRunDepth.TheTwoRunStylesBitsEachAddTheirOwnTerm` new: 2x for a word with both bits; role 7 lists only the Poacher so 1x).
- Red-first (CONFIRMED, commit `df4aa50a` holds only the tests, built against main's source): all 4 controller tests FAIL (poacher/fox words move nothing: 0 vs 5; the Box-to-Box enum moves the spot 4 m; no-row Fox on the switch-off path 0 vs 5) and the additive test FAILS (5 vs 10).
- Mutations (each built, run, reverted by `git checkout`):
  - Ma Fox term dropped in `StyleRunDepth`: RED `FoxInTheBoxInACentreForward...`, `TheTwoRunStylesBits...`, controller `FoxInTheBoxWord...`.
  - Mb Fox term assigns instead of adds: RED `TheTwoRunStylesBits...`.
  - Md sign of the depth flipped at the site: RED `TwoSavedStyleWords...` (−5 vs 5).
  - Me word taken from the enum, row ignored (`StyleWord(GetPlayingStyle())`): RED `TwoSavedStyleWords...` and `FoxInTheBoxWord...` (the two enum-consistent tests stay green, as they must).
  - The old pes-loop-only gate and the old GF table are the two states of main the red run failed against (switch-off no-row Fox: RED; Box-to-Box enum: RED).
  - Not mutated: the `carrier`/`runState` arguments at the site (function-level tests pin them in `pes_offball_test`; no controller test reaches a running or ball-owning forward).
- Not run: the rest of the sim ctest suite and the gate (Main gates): interim line `GATE fast: NOT RUN (Main gates the batch)`; binaries run: `pes_playerdb_wiring_test` 10/10, `pes_offball_test` 77/77.

## 4. Smokes (2 min, seed 7, run directly; machine loaded, sim slots not taken)

| Path | main `83011733` | this branch | ablation B (run-style term on both paths, GF table kept) |
|---|---|---|---|
| switch-off | `2802089ed1b4da32` (reproduced on a rebuild with main's four files) | `4a5f9921d7ce62ef` (rc 0; 10 stoppages, 23181 ticks, idle 18 / stalls 10) | `0ffbb2419b0b8ac3` |
| pes-loop | `e7c1462426cef558` (reproduced) | `f4b283a59fce8403` (rc 0; 24 stoppages, 32698 ticks, idle 3 / stalls 3) | `e7c1462426cef558` |

Both hashes moved: expected, the change alters formation spots. Decomposition (ablation, each build + run this session):
- pes-loop: B equals main's hash, so the Poacher/Fox term was already live there; the whole move comes from deleting the GF depth table for the other 16 styles (fixture players carry inferred enums, so ~every forward/mid had a +/-2..8 m term).
- switch-off: B moves the hash (the Poacher/Fox term now replaces GF's 3 m / 0 m on this path); the rest of the move to `4a5f...` is the same deletion.
- Tripwire status (smoke policy: crash, hang, determinism, zero passes or shots): no crash or hang on either path; "same hash on rerun" was not rerun (single run each); pass and shot counts were not extracted from the logs. Not aimed at any `[balance*]` line.

## 5. Left

1. Bit 4 (Creative Playmaker) `-s * team[+0x3A9C + slot*12]` term and the roles 5-6 `team+0x7B84` gate of L650-653 (need the team record's per-slot vector and that byte).
2. `GetWidthOffset` (`FUN_1405E6E80` L527-582: bit 6 +15, bits 5/6 x0.5, bit 7, bit 18 +15; `fVar47` meaning INF) and `GetPoacherTargetX` (no PES site) still key on the GF enum, as do the four dribble knobs.
3. Controller-level coverage of the carrier and run-action exclusions at this site.
4. Same-hash-on-rerun and pass/shot counts for the two smokes.
