# PES 2021 per-player signature motions: Derivationdatatable.bin, the two walkers, FUN_14071E570's selection (pure, unwired)

Branch `SignatureAnims`: red test `bbe7db0`, module `f391e4f`, merge of origin/main `1aa17c3` as `257d380` (pushed; `git ls-remote` confirms `257d38050c2aa3c9bf51a97d714c8a639208b4b5`).
Source: `docs/re/pes-re-special-players-decode.md` §0-§2. Files: `src/pes/derivation.{hpp,cpp}`, `tests/pes_derivation_test.cpp`, one line in the root `CMakeLists.txt` (pesfmt), one target in `tests/CMakeLists.txt`.

Tags: CONFIRMED = read this pass (bridge decompile and disassembly, opcode bytes from the image, the team's extracted `deriv.bin`, or the install test); INF = inference; OPEN = blocker named.

## 1. Packing (dword0, word1). CONFIRMED
- dword0 bits 0..16, stored id: 0 = generic; else player id = stored - 1. Largest storable id 131070 (stored 0x1FFFF). Asm: `AND EAX,0x1ffff` @0x1406B3662, `TEST R8D,0x1ffff` @0x1406B3859.
- dword0 bits 17..28, motion out = ((dword0 >> 17) & 0xFFF) + 0x3A7. Asm: `SHR R9D,0x11` @0x1406B366D, `MOV R12D,0xfff` @0x1406B35DA, `LEA R13D,[R15-0x23]` @0x1406B35F5 (0x3CA - 0x23 = 0x3A7).
- word1 bits 0..10, motion in = (word1 & 0x7FF) + 0x3CA. Asm: `MOV R14D,0x7ff` @0x1406B35E5, `MOV R15D,0x3ca` @0x1406B35EF.
- word1 bits 11..15, sub = (word1 >> 11) & 0x1F. Asm: `SHR EAX,0xb` @0x1406B3878, `AND AL,0x1f` @0x1406B387B.
- List lengths 8/35/23/15/16/9 for kinds 0..5: `MOV R11D,imm32` at 0x1406B3595, 0x1406B35AD, 0x1406B35A5, 0x1406B359D, 0x1406B35B5, 0x1406B35BD (jump table @0x1406B3744 checked). Walker 2 agrees (0x1406B37B5..0x1406B37DD).
- **Bits above the fields are set in the real file, and the walkers ignore them.** Entry 8 (list 1, player 990) has dword0 `0x35B803DF` (bit 29 set). Entry 77 (list 5, first row) has `0xAB500000`. The 12-bit mask decodes them to 3715 and 2383. Checked against bytes in the team's `deriv.bin`: `df 03 b8 35 82 05 00 00`, `00 00 50 ab 82 15 00 00`. Entry 0 (`a1 9d 00 00 08 00 00 00`) is Neymar's 978 -> 935.
- The decode doc's "sub id" column is the raw 5-bit field (list 5 entry 0: word1 `0x1582`, sub 2). The generic walk matches raw - 1 == stat (`DEC AL; MOVSX EAX,AL; CMP EAX,EBP` @0x1406B387D..0x1406B3884).

## 2. FUN_1406B3560, the player walk and its tail. CONFIRMED (asm 0x1406B3560..0x1406B3742)
- Scan: an entry whose motion in matches and whose id matches sets the result. The scan stops at the first entry that changes the result. An identity entry (out == in) does not stop it.
- **CORRECTION to decode doc §0:** playerId 0 skips the id test (`TEST ESI,ESI; JZ 0x1406B367F`). Any entry with the motion in answers, player or generic. The doc's `(id) - 1 == playerId` rule holds only for playerId != 0.
- Tail (ids 134563 @0x1406B36C1 and 134448 @0x1406B36C9, which have no list rows):
  - motion 1233 (@0x1406B36D1), kind 2 -> motion + 3 (`LEA EAX,[R8+3]` @0x1406B36DF) = 1236;
  - motion 1248 (@0x1406B36FB), kind 2 -> + 3 (@0x1406B3709) = 1251;
  - motion 2380 (@0x1406B3725), kind 1 -> 2367 (`MOV EAX,0x93f` @0x1406B3731).
  The tail overrides the list result. It does not read the scan's output.

## 3. FUN_1406B3780, the generic walk. CONFIRMED (asm 0x1406B3780..0x1406B38D2)
Entries with id field 0 only; motion in matches; raw sub - 1 == stat; same stop rule; a kind outside 0..5 returns the motion in.

## 4. FUN_14071E570, the long-kick selection. CONFIRMED (asm 0x14071E570..0x14071E6E4)
1. Beckham: id in {815 (0x32F), 118718 (0x1CFBE), 123447 (0x1E237), 134448 (0x20D30), 134563 (0x20DA3)}; flag == 0; picked in {2365, 2366} (`LEA EAX,[RDI-0x93d]; CMP EAX,1; JBE` @0x14071E60B..0x14071E614) or 2376 (`CMP EDI,0x948` @0x14071E616); vt+0x28 of player+0xAB8 == 8 (`CMP EAX,8` @0x14071E62B). Result 2367 (`MOV EAX,0x93f` @0x14071E630). Early return, no remap.
2. Else, if substitutable: player walk kind 3 with the picked clip. If it left the clip unchanged: a curve (2365) is walked as 2380; generic walk kind 1 with stat = FUN_1406E6660(player, 0xE). If the generic result is 2380, the function returns 2365 (`LEA EAX,[R14-0xF]` @0x14071E6AF).
3. Remaps last, on whatever clip resulted: 2654 -> 2654 + 0x1D = 2683 (@0x14071E6B5, 0x14071E6BD); 2750 -> 0xADC = 2780 (@0x14071E6C7, 0x14071E6C2).

## 5. Constants and loading. CONFIRMED
- Every constant is an immediate or displacement of the exe's instructions, read through `PeImage::ReadVa` and opcode-checked: 55 sites (layout 27, rules 28). Duplicate sites must agree (walker 2 vs walker 1 packing and lengths; selection copies of 2365, 2380, 2367 and the Beckham ids vs the tail's). No PES number table is in code or tests.
- The file is read through the VFS winner: `common/anime/playerID/bin/Derivationdatatable.bin` (the path the team's `pesfmt_dump` uses).

## 6. Install results. CONFIRMED (install test run, `PES21_ROOT` set)
- 848 bytes, 106 entries, and every motion number names a clip in `MotionNames`.
- All five Beckham cards: fk_idle -> fk_idle_beckham_2002 and fk_runup -> fk_runup_beckham_2002 (kind 2); curve placekick -> curve_beckham_2002 (kind 1). The two tail cards get these from the tail; the other three from list rows (entries 19-22).
- Selection: for each Beckham card, picked 2365, 2366 or 2376, flag clear, variant 8 -> curve_beckham_2002. Flag set -> not.
- Neymar: list 0 row 0, feintrun_doubletouch_3_3_f090 -> feintrun_ballroll_onetime_3_3_f090_y0_Ltouch_in_ver03.
- List 5 (placekick): stat 1 (sub 2) -> curve_silva; stat 2 (sub 3) -> curve_ozil; stat 9 (sub 10) -> curve_coutinho.
- The five `download/` 4cc packs in this install do not ship the table, so the install tests ran rather than skipped. A test skips when a download pack does ship it.

## 7. Verification
- Red (before the module, `bbe7db0`): `fatal error: pes/derivation.hpp: No such file or directory`.
- Green: `PES21_ROOT=<install> ./build/tests/pes_derivation_test` gives `[  PASSED  ] 22 tests` on `f391e4f` and again on the merge `257d380`. 15 synthetic tests, 7 install tests.
- Mutations (one per test; each applied to `src/pes/derivation.cpp`, rebuilt, run to a failure of its own test, then reverted. The reverted file had its original content):
  1. size check with a 7-byte stride: `RejectsAFileThatIsNotWholeEntries` fails (24-byte file rejected).
  2. generic id decoded as player -1: `IdFieldZeroIsGenericAndIdFieldOneIsPlayerZero` fails.
  3. id mask 0xFFFF: `TheSeventeenBitIdFieldKeepsEveryIdUpToItsLimit` fails (131070 -> 65534).
  4. motion-out mask dropped: `MotionAndSubFieldsAreMaskedAndJunkAboveThemIsIgnored` fails (21643 vs 1163). Also covers `InstallEveryEntryNamesAClipInTheInstallsMotionTable` (7811 past 4389 names).
  5. player-id test dropped: `MatchesTheIdAndTheMotionInAndTheFirstChangeWins` fails (610 vs 620).
  6. scan stops on any match: `SkipsAnIdentityEntryWithoutStoppingTheScan` fails (600 vs 640).
  7. zero id skips nothing: `PlayerZeroMatchesEveryEntryByMotionOnly` fails (700 vs 750).
  8. tail defers to the list: `TheTailRewritesItsTwoCardsOverTheListResult` fails (999 vs 534).
  9. sub compared without minus one: `GenericWalk.MatchesTheStatPlusOneAgainstTheSubField` fails. Also covers `InstallListFivesPlacekickVariantsResolveByTheirSub` (stat 1 -> placekick, not silva).
  10. player rows admitted to the generic walk: `GenericWalk.IgnoresEntriesThatNameAPlayer` fails (512 vs 510).
  11. flag ignored: `TheBeckhamBranchNeedsAListedCardACurvePickAFlagClearAndTheAnimeVariant` fails (520 vs 500).
  12. generic walk skipped: `ASubstitutableClipTakesThePlayerWalkThenTheGenericWalk` fails (600 vs 660).
  13. curve walked unmapped: `TheCurveWalksTheGenericListAsThePlacekick` fails (500 vs 515).
  14. round trip returns the placekick: `TheCurveReturnsToItselfWhenThePlacekickWalkFindsNothing` fails (510 vs 500).
  15. remaps disabled: `TheRemapsRewriteTheirClipsWhateverTheWalksSaid` fails (800 vs 807).
  16. parser skips the last list: `Install.TheTableHoldsItsSixListsAsWholeStockEntries` fails (list 5 empty; 97 entries vs 106).
  17. tail offset +1: `Install.BeckhamsFreeKicksSubstituteByName` fails (134448 and 134563 get fk_idle_cristiano).
  18. tail curve rewrite dropped: `Install.BeckhamsPlacekickSubstitutesByName` fails (134448 and 134563 stay on the placekick).
  19. curve range exclusive: `Install.BeckhamsCurvedKickSelectsTheBeckham2002ClipByName` fails (curve_beckham, 2366, not Beckham).
  20. motion-in base +1: `Install.NeymarsFeintIsItsOwnDoubleTouchSoleClip` fails.
  All 22 tests are covered. Runs 4 and 9 are the shared mutations for the install tests named above.
- Build: all 80 test targets (`tests/` and `tests/sim/`), `gfsim_run` and `pesfmt_dump`, on the merged tree: rc 0, zero compile errors.
- Gate: not run (Main gates merged batches).
- Smoke (`gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 2 --pes21-root PES21_ROOT`, plus `--pes-loop` for mode B):
  - base binary (origin/main `ea49faa`, before the module): A `[determinism] 961c600e97796617`, 33164 ticks; B `[determinism] 2c78318c734ae1e1`, 41582 ticks.
  - this branch before the merge (`f391e4f`): A `961c600e97796617`, 33164 ticks (identical); B `2c78318c734ae1e1`, 41582 ticks (identical; only the wall-clock `[profile-split]` line differs).
  - merged tree (`257d380`, origin/main `1aa17c3`): A `961c600e97796617`, 33164 ticks (identical to base); B `182e4fe85687c8db`, 40528 ticks (differs from the base B).
  - The B difference is origin/main's landed batch, not this module: `gfsim_run` contains no derivation symbols (`nm` finds 0 matches for the module's functions), and no source outside the module and its test includes `pes/derivation.hpp`. I did not run origin/main itself. Main's merged-batch smoke should confirm.
- Machine-path scan of changed files: no matches.

## 8. Open items
1. OPEN: FUN_1406B2080 (the substitutable bit, from the anime record table) is not ported. The caller passes `LongKickInput::substitutable`.
2. OPEN: FUN_1406E6660(player, 0xE), the stat byte (index 0xE at 0x14071E67D), is not ported. The caller passes `LongKickInput::stat`.
3. OPEN: FUN_1406EA7B0 (player id), FUN_1406ABB30 (picked clip) and vt+0x28 of player+0xAB8 (anime variant) are the caller's inputs.
4. OPEN: the code that reads the list file (the exe names it at 0x1425DC952) is not isolated. The port reads it through the VFS winner, as the game does.
5. INF: the free-kick and penalty lists (kinds 2 and 4) are exercised only by the walker tests and the table test. Their callers are not ported.
6. Not done here: wiring into clip selection (out of scope for this ticket).
