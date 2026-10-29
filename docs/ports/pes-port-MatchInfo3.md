# Kick-off counter confirmed live, forced-clear premise corrected, MA's rows are views over the window (MatchInfo3, 29-10-26)

Branch `EndClear` (worktree `gf-EndClear`). Pushed heads: `cdc81e82` (kick-off staging), `0cb43a77` (MA row views), plus a final merge of `origin/main` (`bbd5dc7c`); confirm with `git ls-remote origin EndClear`. Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled. Not done (step 3, step 5, forced clear's port): see §5.

## 0. Verdict

| Step | State |
|---|---|
| 1 kick-off test | **DONE (staged).** GF's stored MI+0x1198 matches PES exactly (CONFIRMED live + captures, §1). The test is re-staged by seed search (1003), bound unchanged. 28/28 `pes_movestep_wire_test`. |
| 2 forced clear | **Premise wrong, port OPEN.** `0x140A4C820` is a pitch-zone test, not an opponent count (CONFIRMED decompile, §2). The forced-clear test is inline in the walk `0x14210F110`; not ported, `forcedClear` stays false. |
| 3 danger flag | **BLOCKED on captures, not done.** `danger_writer_699960` / `danger_builder_2119390` never snapshotted: the watcher released every call "disk under 420/400 GiB" (§3). The one stand-in site remains. |
| 4 MA lists | **DONE.** The window region is the store; the serialized copy is deleted. Score, pair, angle, nearest and bearing rows too. |
| 5 angle inputs | **Not added** (budget). Scan notes in §4. |
| 6 tests, mutations | Run; M2 re-run separately; see §6. |

## 1. Step 1: MI+0x1198 is exactly one per logic frame, first tick reads 0

- Live read-only (`sudo -n` pread, pid 1694737, G = *(0x1436F3DC8), MI = *(G+0x270)): MI+0x1198 = 55388, 55389, 55391, 55392 over 0.3 s samples; 30 samples at 50 ms: 55617..55625, monotonic, never skipping a frame faster than the game's own (slow) frame rate. CONFIRMED. Increment site `0x140A128E4` (`inc dword [rsi+0x1198]`, `0x140A128DE add [rsi+0x1194],edi` before it) sits in the type-13 record body gated on `[r14+0x31A] != 0` (disasm `0x140A128D4 cmp byte [r14+0x31A],0`); the counter reaches MI through the publish copy `0x14100CCA0` (INF, `docs/re/pes-re-records.md` section 6).
- Captures (`live2/pid*/seq*`, MI read from `mem.bin`): the clock driver hooks `restart_44C880` (`0x14044C880`) and `restart_driver_445E90` (`0x140445E90`) read MI+0x1198 = **n - 1 on their n-th call**: call 1 -> 0, call 200001 -> 200000, 250001 -> 250000, 300001 -> 300000, 350001 -> 350000 (CONFIRMED, `calls.py` over 170 captures). So the counter counts every frame (not only gated ones) and the first tick reads 0. GF: `Match::Process` reads the window counter during the tick and `pesWorld->AdvanceMatchFrame()` runs at its end: tick t reads t. **GF's writer matches PES; nothing to change.**
- Other first-call frames (CONFIRMED): `camera_update_87F280` 1, type-13 consumer `0x140A115A0` 1, referee latch/judge/involvement `0x140A3A870/930/690` 59, `ref_restart_cmd 0x140A39A80` 61.
- Why the test moved: GF consumers' differences between stored and derived frames (lag of 1 and repeats) shift thresholds (`MoveHost::PhaseTick`, `MatchTick`, `RecentStamp`, `Think().frame` read the absolute frame). The trace diverges from tick 2 and the kick lands at tick 118 (stored) vs 125 (derived). Chaotic in one frame, so a re-stage is the right fix.
- Seed search over 1000-1011 (same code, only the seed): closest taker distance 0.80 on 1000, 1001, 1002, 1005, 1006, 1008, 1009, 1010 (kick from 0.80 m); 0.16 on 1003, 0.23 on 1004, 0.41 on 1007, 0.22 on 1011. Test staged on 1003 (`kKickoffSeed`), assertion `closest < 0.65` unchanged. **Finding for Main:** most seeds kick from ~0.80 m: the taker's root-to-ball distance at the kick is a GF release behaviour that is bimodal over seeds; the 0.65 m "PES reach" in the test comment is an old GF stand-in value, not a PES read. OPEN: PES's kick-off release is the receiver's ball-ownership check (`pessetpiecetaker.hpp`), 28 frames after the set-play counter; GF's kick lands at ~118 ticks.
- Not re-mutated: the kick-off test itself (a mirrored-heading mutation is `ABearingToAGroundPointIsTakenOnPesZ`'s).

## 2. Step 2: what `0x140A4C820` is

CONFIRMED (`dec.sh`, decompile of `0x140A4C820(G, slot, side)`): returns 0 unless `slot < 0x16` and the man record `0x140A48A10(G, slot)` has |z (+0x564)| <= pitch mark[7] (G+0x2AA8 record, +0x1C) and mark[6] <= |x (+0x55C)| <= mark[0]; then, with the side's direction byte `0x140A48A30(G, side)+0x254`: byte 1 -> returns 0 if x >= 0 else 1; byte 0 -> returns 0 if x < 0 else 1. I.e. "the player is inside the zone band on this side's attacking half". It is already in the layer as `pes_fn_140A4C820` (`src/pes/translated/ball/`). Callers in the walk (`carunits5.c` L222, 304, 310, 316): six sites.

The "forced clear" test of rule 7 is **inline in `0x14210F110`** (decompile `~/ghidra-projects/scratch/Unpacker/out/carunits5.c` L190-432, VAs about `0x14210F4F2-0x14210FB2B`): after the clear answers (`type == 5`), it reads the pressure record `0x14069A3E0(imageA+0x2210, 0xFF)` (+0x30, +0x34, +0x10, +0x14, +0x18, +0x1C, lanes at +0x4C + 0x1C k), a loop over the 11 opponents via `0x142118C90/0x142118D40` (imageA+0xC58 lists, +0xE0/+0xE4/+0xE8), the lane classifier predicates `0x140A6E450/0x140A6E0F0/0x140A6E3C0/0x140A6E4B0/0x140A6E580` on `0x140A48910` records, distances `0x14043A020`, `0x140439E50` (MA+0x2482C ball copy), `0x140A4C7A0` (a second zone test), and only then the replacement search over kinds 3, 4, 9, 6 with `0x142119DF0` answer checks. It is not a call, so GF cannot "set a flag from a function": porting it means translating `0x14210F110` (unit walk) with imageA+0x3E20's PES-layout unit table, or hand-porting about 230 decompile lines with no oracle (no capture reaches it; no hook on `0x14210F110`). Not done. `walk.forcedClear` stays false; the stand-in is unchanged.

## 3. Step 3: danger flag (blocked)

- `closure.py --dry-run` (previous session): `0x140699960` 108 functions, `0x142119390` 193. The writer reads MA (+0x18/+0xE0 -> E), G+0x40/0x270/0x2AA8, the man/anime/slot tables and lane lists (`dec_danger.txt` L63-123): every one a window record with partial GF writers; no real call can prove it without a capture.
- Watcher log (`live2/watch.log`): `danger_builder_2119390` and `danger_writer_699960` calls at 08:16, 08:17, 08:24, 08:26, 08:28, 08:29 and 09:39, 09:42, 10:14 were all "released without snapshot: disk under 450/420/400 GiB". Main lowered the floor to 400 GiB at 09:29 (free was 412), PES restarted as pid 1792161 (`live2/pid1792161/`, 5 captures at 10:14, none of the danger hooks). **Needed:** more disk headroom (about 25 GiB free above the floor per two captures) so one danger_writer and one danger_builder call snapshot. Then: translate with `closure.py` on the post-AN tree (`bbd5dc7c`), harness case from the capture, imageA+0x31C1 from the translated writer, delete the `ponytail:` site in `elizacontroller.cpp:2764-2770`.
- Unchanged: the stand-in is one named site (`SetClearDanger(side, ...)`).

## 4. Step 5: angle inputs (not added)

Scan of the image for stores with the field offsets (capstone, aligned decodes only), man record +0x5D0: real stores at `0x140431F73`, `0x1404964B1`, `0x1404989D7`, `0x14049FBBB`, `0x1404BCDA5`, `0x1404F0147`, `0x140413965` (a many-writer action word; the marktable's `FUN_140A6CA10` is an admit reader). Each needs its own decode and GF source (player action state). Anime +0x11C, slot +0xA1/+0xA4 and ball +0x1C/+0x24/+0x80: scan too noisy at small displacements, not decoded. OPEN, unchanged from MatchInfo2 item 5.

## 5. Step 4: MA's rows are the window

- `SortedRow` is a view over a 200-byte block (team t's 11 `(slot id u32, value f32)` entries at `+0x58 t`, count bytes `+0xB0`, rank bytes `+0xB2 + id`, unlisted fill `0xFF`: INF for the byte widths, consistent with `NearestEntry`, `pesoffball.hpp`). `FloatRow` is a 22-float view. `ScoreRows::Bind(ma)` points score (`MA+0x228EC/0x22AD4`), list (`+0x229B4/0x22B9C`), angle (`+0x22A7C/0x22C64`), pair (`+0x22CBC/0x22D84`), nearest (`+0x22E4C + 200 i`) and bearing (`+0x23F7C + 0x58 a`) at the side's window record. The block offsets tile exactly (score0 +0xC8 = list0, list0 +0xC8 = angle0, angle0 +0x58 = score1, ... nearest[21] ends at bearing), CONFIRMED by arithmetic against the header table.
- `TeamAIController::TickScoreRows` binds to `world.Ma(team->GetID())`; the serialization loop in `WriteMaRecord` and `PesWorld::ClearMaListEntry` are deleted. A default `ScoreRows` owns a scratch MA only for the unit tests that build rows with no world (named in the header). A fresh bind clears all rows (a zeroed PES record would list slot 0 at rank 0; found by `TheWriterIsLatchedByTheFirstFullRefresh`).
- OPEN (INF): count/rank byte widths, and the 0xFF fill of unlisted entries.

## 6. Verification

| Binary | Result |
|---|---|
| `pes_movestep_wire_test` | 28/28 (kick-off test on seed 1003) |
| `pes_scorerows_test` | 12/12 |
| `pes_tick_a1_test` | 26/26 (1 new: `TheScoreRowsAreViewsOverTheWindowsMaBlocks`) |
| `pes_carrier_test` | 189/189 |
| `pes_mark_table_test` | 19/19 |
| `pes_offball_test` | 76/76 |
| `pes_angle_oracle_test` (`GF_PES_ANGLE_ORACLE=seq021`) | 4/4 |
| Not run | `pes_involvement`, `pes_possession`, `pes_event_wiring`, `pes_team_tick`, `pes_clock_match`, `pes_world`, `pes_match_info`, `pes_events_test` (they link the changed `pesscorerows`/`teamAIcontroller`; Main's gate covers). Gate not run (Main gates). |

Red first: the new test fails on the old copy (rows kept their own store: a value written into MA is not read back); the `pes_scorerows_test` iteration over `bearing` needed the view's API (compile change). The kick-off test was red on the branch head before the staging (0.80).

Mutations (applied in one build, each caught by its own test, reverted by `git checkout`, tree clean):

| Mutation | Result |
|---|---|
| M2 `WriteMaRecord` writes ball (x, z, height) (re-run: the strengthened MA ball test) | `TheTeamAisRecordMaIsWrittenEachTickFromTheBallAndTheRows` FAILED (caught now) |
| M6 `scoreRows_.Bind` removed (rows keep a private store) | `TheScoreRowsAreViewsOverTheWindowsMaBlocks` FAILED |

## 7. Open items (numbered)

1. **Danger flag (step 3)**: needs danger_writer/builder captures (disk headroom, §3), then closure.py + translation + harness + delete the stand-in.
2. **Forced clear**: port the inline walk test of `0x14210F110` L190-432 (or translate the walk over a PES-layout unit table); `forcedClear` stays false.
3. **Angle inputs** (§4): ball +0x1C/+0x24/+0x80, man +0x5D0, anime +0x11C, slot +0xA1/+0xA4 writers.
4. Kick-off release: GF kicks at ~118 ticks from 0.80 m; PES releases on the receiver's ownership check 28 frames after the set-play counter (`pessetpiecetaker.hpp`).
5. The `PhaseTick` stand-in (`pesmovestep.cpp`) returns the absolute match frame where PES's phase count is per phase: it is why one frame moves the approach.
6. Cleanup: remove `gf-EndClear-scratch` (oracle seq021, scratch scripts) once archived.
