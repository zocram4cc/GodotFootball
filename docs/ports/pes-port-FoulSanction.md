# PES port FoulSanction: PES's sanction tree 0x14047F170 runs on GF's world window and decides the foul scene; proven 7/7 on real calls; the lossy level fold and the constant scene are gone (29-10-26)

Branch `FoulSanction`, worktree `gf-FoulSanction`, based on `origin/main` `a57c21f5` + a merge of `origin/FoulOffside` (the translated tree, its closure and specs were only on that branch; clean merge). Labels: CONFIRMED = tool output or test this session; INF = inference; OPEN = blocker named. Captures are PES-derived and stay outside the repo: `gf-Rendezvous-scratch/live/seq084,086,088,090,096,098`, `live2/seq109`.

## 0. Status against the ticket

| item | status |
|---|---|
| 1. worktree, godot-cpp symlink, push early, read prior work | DONE (FoulOffside, SanctionSwitch, SanctionCaller, open-decodes, CutsceneHybrid4 read; merged FoulOffside) |
| 2. decode `0x14047F170` | DONE (section 1) |
| 3. translate + prove bit-exact on recorded cases | DONE: snapdiff 7 of 7 calls bit-exact (rax and every written byte), own-block coverage 0.5412 (105/194). Results: `tools/dev/translate/results_snapdiff_foul.json`. The translation already existed (FoulOffside: `impure_sample/pes_fn_14047F170.cpp` + closure); no new function was needed |
| 4. GF: sanction from the tree on PES layout; delete the GF rule; red-first test; mutation; expose the kind | DONE for the scene decision and the level fold (section 3). NOT replaced: the judge's card level, the penalty decision, the card's timing (they are upstream of the tree in PES: section 4) |
| 5. fast-tier line, determinism smoke | not run (Main gates; no `GATE_TIER` on this branch); interim line at the end |

## 1. The tree (CONFIRMED: bridge decompile `dec_47F170.txt`, translated body, replays)

`0x14047F170(A0 = req, A1)` returns the **scene id** in eax; its only caller chain is builder `0x140480910` -> `0x14047E940` -> tree (`pes-re-sanction-caller.md`). Sanction **kind** is encoded in the scene id; it is a function of:

- **MI** (`G+0x270`): `+0x1168` period state, `+0x1554` restart kind (6 = penalty picks scenes `0x2000F..0x20014`), `+0x155C/0x1564` spot, `+0x158C` card level (1 foul, 2 foul + warning, 3 yellow, 4 red), `+0x1590/0x1594` fouler/victim slot, `+0x15B8`.
- **G**: `+0x2AA8` field info (two f32: 52.56, 34.06 in every capture = pitch half sizes + 0.06 line half-width), `+0x1928` side table (`+0x254` attack sign), `+0x1978` side-B team info (per-squad-place record index at `+4`, records at `+0xA8` stride `0x128`: **`+0` = the player's yellow count, the card being decided included** (seq090: only the fouler holds 1; seq109 likewise), `+0x34` nonzero id = present).
- **A1** `+0xE0` -> event store E, ring 4 newest record (`0x140A3E150`; `+0x00` kind 0x1E, `+0x10` period state, `+0x18` foul kind (2 direct, 3 indirect), `+0x1C` card level, `+0x28` side, `+0x42` injury byte); `A1+0x18 + 0x24A0C` per-side 0x14-byte records (dword `+8 == 3`); card history `0x140A81230` (E stats at `+0x750D0`/`+0xCE720`, key 5 = sum of eight encoded counters via `0x1408C9190`; G+0x2AD0 competition object).
- **A0** `+0xD`: played-scene counter.

Decision structure (decompile): fouler in own half beyond |x| > 31.0 (`0x1425A2500`, `0x14047F6CC`) -> flag; card level 2 -> `0x20006` (fouler already booked) / `0x20008`; level 3 with `MI+0x15B8` and booked -> `0x20007`; otherwise the main group by (card colour, flag, yellow count): yellow `0x20001` (first) / `0x20004` (booked), red `0x20003` / `0x20005`, flagged group `0x20009/0x2000A/0x2000B/0x2000C/0x2000D/0x2000E`, penalty group, late-foul group `0x20015..0x20018` (leading team, period 3 minute gate), injured-victim group `0x20019..0x2001E`; default `0x20022` = **no scene**. A plain foul (level 1) in the open is `0x20022`. (Scene names are mine; values are the tree's immediates.)

## 2. Proof (CONFIRMED)

`frame_trace.py recorded` over the seven captures, then `snapdiff.py --only 0x14047F170` (harness runs PES's bytes, the translated function runs on the same snapshot): 7 calls, 7 replayed, 0 excluded, 0 mismatches, proven. PES's answers: seq084 `0x20022`, seq086 `0x2000B`, seq088 `0x20022`, seq090 `0x20009`, seq096 `0x20022`, seq098 `0x20022`, seq109 `0x20001`. Hooked caller = the tree itself (`foul_sanction_47F170`); no other hook covers it. Coverage is low on level/late branches: all seven are level 1 or 3, free kicks, no penalty/red/injury/late-game. Main added the hook to the watcher allowlist (rvwatch12); no new capture had landed when I stopped (`live2/pid1981120` has none). Rerun: `trace.sh` / `snap.sh` pattern in the report's method line, add the new seq dirs.

## 3. GF changes

- `pesworldrecord.{hpp,cpp}`: the world window gains the tree's records in PES layout (`PesSanctionImage`: request A0, A1, A1 block, field info, per-side team info, foul-event source, event store E **last** so the counter tables at E+0x750D0 read zero past the window). `G+0x2AA8` and `G+0x2AD0` named. API: `SetFieldInfo`, `SetPlayerInfo(slot, id, yellows)`, `PushFoulEvent` (the translated ring push `0x140A3DAC0` runs on the window), `DecideSanction()` (the translated tree; answer stored at `req+0` as the builder does), `SanctionScene()`.
- `referee.{hpp,cpp}`: `Foul::level` keeps PES's judge level 0..4 (the old fold `{0,1,1,2,3}` applied in `JudgeContact` and its inverse `{0,1,3,4}` in `CheckFoul` and in the `PublishPesStoppage` mirror are deleted; the card type is derived once in `RecordFoul`). The mirror now publishes the real level, so a **warning (level 2) reaches the tree**; the advantage decision uses the real level. `DecideSanction()` stages the foul event, booking counts (from `GetCards()`) and field info, and runs the tree once per foul stop (`sanctionDue` set where the whistle is blown, run after `PublishPesStoppage` in the same `Process`).
- **Accessor for the cutscene request (CutsceneSelect lane, not edited): `Referee::SanctionScene()` (u32 scene id, `0x20022` = none) and `Referee::SanctionSerial()` (counts decisions; compare to detect a new one).** `GfMatch`'s constant `0x20008` for `FreeKick`/`Penalty` stops (the ponytail naming `0x14047F170`) should read `match->GetReferee()->SanctionScene()` and skip the cutscene when it is `0x20022`; the foul stop it already detects from `RefereeBuffer` edges matches `SanctionSerial()` incrementing.
- ponytail stand-ins (each in code with its PES site): player id `slot + 1` (PES: squad loader writer of `rec+0x34`, not decoded); place map identity (PES: permutation, loader not decoded); `A1.18 + 0x24A0C` records zero (writer not found); E card/goal counters zero (encoded store, writer of `0x1408C9190`'s cells not decoded) so the leading-team late-foul scenes never fire; competition object null; `A0+0xD` zero (first decision of a stop); foul-event stamps `+4/+8`, `+0x14`, `+0x20`, `+0x24`, `+0x2C` not written (the tree does not read them).

## 4. What is NOT replaced (OPEN)

The tree takes the card level as input. PES's **judge writes `MI+0x158C`** (writer OPEN, SanctionCaller 8.5); GF's `JudgeContact` -> level remains. The penalty decision (`MI+0x1554 == 6`) is GF's `CheckFoul` geometry (`20.15 - lineHalfW`, `pitchHalfW - 16.5 + lineHalfW`), PES's own writer not decoded. The card's timing (`GiveYellowCard(stop + kPesCardOnScreen_ms)`) is GF's; in PES the scene sets it. Late/deferred cards (`CardBook`, p5 = 1 step) and the injury step (p6 = 1) do not call the tree yet. So "none / warning / yellow / red" is still decided by GF's judge; what PES's tree now decides in GF is the scene/no-scene and which ceremony, and the level is no longer lossy.

## 5. Tests (`tests/sim/pes_sanction_test.cpp`, 10 cases, all pass)

Red-first: on `origin/main` the target does not exist (`PesWorld::DecideSanction` missing: feature red). Four cases mirror recorded calls (seq084, 086, 090, 109: the facts the capture held staged on the window; expected = PES's returned scene): plain foul no scene, first yellow, a foul 0.0077 m past 31 m (flag scene) and the boundary 0.01 m inside it (no scene), deep yellow. Others follow the decompile: warning (GF's old fold gave a plain foul; PES `0x20008`), warning on a booked player `0x20006`, second yellow `0x20004`, red `0x20003` / booked `0x20005`, the answer is stored at `req+0`.

Mutations (applied, rebuilt, run, reverted; file hash back to baseline): M1 card level not written into the ring -> 5 failed; M2 yellow count ignored -> 3 failed (warning-booked, second yellow, red-booked); M3 player id zero -> 7 failed. Not mutated: the `Referee` wiring (`DecideSanction`, the mirror level, `RecordFoul`): no test drives a Match to a foul stop with a card (cards are rare in short matches; `pes_stoppage_timing_test`'s match test aborts in this Debug build on `humanoid.cpp:2121 desiredLookAt.coords[2] == 0`, an unrelated assert; main is Release). Guarded by compile + link only.

## 6. Checks run

`pes_sanction_test` 10/10, `pes_involvement_test` 6/6, `pes_match_info_test` 2/2, `pes_judge_test` 22/22 (Debug build, `PES21_ROOT` set, rerun after the regeneration). `gfsim_run` links. `pes_stoppage_timing_test`: unit cases pass; `AMeasuredStopRunsPesBracketNotFifteenSeconds` aborts on the unrelated Debug assert above (not run to completion). No smoke (Main). `gen_translated.py --check` on the merged tree reported two drifts (`pes_dispatch_gen.hpp`, `translated.hpp`: main's headers lack FoulOffside's functions); regenerated on this branch (`cda556f0`: only those two headers change, +24 lines; they now declare `0x14047F170` and `0x140A3A690`, so the two ponytail forward declarations in `pesworldrecord.cpp` are deleted). Regenerating touches every layer file's mtime (full 4k-object rebuild). Fast-tier line (interim, no `GATE_TIER` on this branch): the four binaries above GREEN; `gen_translated --check` RED before the regeneration (2 header drifts), not re-run after it (the generator wrote those headers); gate not run (Main gates).

## 7. Open items (numbered)

1. Cutscene request reads `SanctionScene()` (CutsceneSelect lane), deleting its constant `0x20008`.
2. A Match-driven wiring test (needs a Release build or the Debug assert fixed, and a seed that books a player).
3. PES writers: judge level `MI+0x158C`, `MI+0x15B8`, the encoded goal counters at E+0x750D0 (leading-team scenes), `A1.18+0x24A0C`, the kind-4 record fields `+0x14/+0x20/+0x24/+0x2C`, player id / place map.
4. Tree calls from the late-card step (p5 = 1) and injury step (p6 = 1), and `A0+0xD` from the builder.
5. New captures (red, penalty, injury, period 3) when the watcher delivers: rerun the replay and append to `results_snapdiff_foul.json`.
6. The Debug-only `humanoid.cpp:2121` assert (a Debug match aborts).
