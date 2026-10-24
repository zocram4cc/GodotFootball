# PES free kick and penalty SetKick placement (FKPenaltyPlace)

Status: DONE on `FKPenaltyPlace` (head `f3facbd`, pushed; merged with origin/main `b7cd787`). The full gate is not run here (Main gates).

## What PES does (raw bytes, `pes_full.bin` file offset = VA - 0x140000000)

- **Free kick, `FreeKickLoop::SetKick` 0x1407411F0.** Run-up cell (row 0x509, column 9) = `fk_runup` (1248). The base is walked by the player remap (list kind 2), then the stat walk when that changed nothing. The player walk is gated on `MI+0x1740 > 0x15` (CMP EBX,0x15 at 0x741289 and 0x7412DD). A second kicker (ids 0..21) skips the player walk and zeroes the FK stat (CMOVBE at 0x741289). The offset is FUN_1406E8CC0 at index 0, negated in x and z. The yaw is FUN_1406B1C70 with the mirror flag, folded once to [0, 360). The stand is FUN_140732D40. The tail (0x741473-0x741564) pulls a stand within 0.5 m of the pitch corner in on the axis that overshoots more, to its mark less 0.5 m (0x142565600). It then clamps |x| to F0 + 1.5 m and |z| to F1 + 1.5 m (0x14259BDDC).
- **Penalty, `PenaltyKickLoop::SetKick` 0x140782930.** Run-up cell (0x8EB, column 9) = `pk_runup` (2926). The player walk (list kind 4) runs unconditionally, then the stat walk when it changed nothing (0x1407829CC-0x1407829EB). No tail. The stand composition is the same.
- **Restart yaw (record +0xC).** `FUN_14092E7E0` takes it from `FUN_1408B4560` by MODE (MI+0x1554). MODE 5 (free kick): the bearing from the spot to (attackSign * F0, 0), where the attack sign is T+0x254 of the taker's side. MODE 6 (penalty): 90 when the spot's x is at least 0, else 270 (0x14259BE5C, 0x1425A2008). The wrappers `FUN_1406B3EE0` and `FUN_1406B43E0` copy the normalized yaw into the spot struct's +0xC.
- **Heading.** `FUN_140732D40` calls `FUN_1404BEF90`: the restart yaw folded, plus the contact yaw, folded again.

## Correction to `docs/re/pes-re-motion-remap.md` (RemapRE)

The remap doc reads `uVar13 = *(MI+0x1740)` as the taker's anime-record count, so a default taker "skips the player walk". The field is the published second kicker: `FreeKickLoop` goes to phase 2 when `MI+0x1740 < 0x16` (`docs/re/pes-re-kicker-standby.md` section 3), and the live watch logged `MI+0x1740 = 255` (none) across 47 records (`docs/re/pes-re-setpiece-kicker.md:116`).

So a default taker with no second kicker runs the free kick's player walk. Card 0 matches the first kind-2 `fk_runup` row, which is `fk_runup_robecal` (1261), not `fk_runup` (1248). The remap doc's default-FK conclusion and its mutations 1 and 2 are inverted for that case. The port pins the stock result by name (`DerivationInstall.ADefaultTakersRunUpsAreTheStockRows`: `fk_runup_robecal` and `pk_runup_neymar`). Its mutation tests are kept, and they check the gate as the bytes read it. RemapRE and Main should review the doc.

## Port shape

- `src/pes/derivation.{hpp,cpp}`: `SetPieceKeys {cardId, stat, secondKicker}`, `kNoSecondKicker = 255`, `kSecondKickerIdLimit = 0x15`, `DeriveFreeKickRunUp`, `DerivePenaltyRunUp`. The penalty does not read `secondKicker`, which is the point of mutation 3.
- `src/sim/onthepitch/pesgoalkick.{hpp,cpp}`: `FreeKickRestartYawDeg`, `PenaltyRestartYawDeg`, `ContactYawDeg`, `SetKickHeadingDeg`, `FreeKickFieldClamp`. The corner's functions are unchanged.
- `TeamAIController` (`teamAIcontroller.{hpp,cpp}`): `PlaceRunUpStand` (the shared placer), `PrepareFreeKickStand`, `PreparePenaltyStand`. `PrepareSetPiece` places and holds both takers on the PES path. The GF stand-ins (2.3 m, and 3.0 m for the penalty) remain on the off-PES path only.
- Law 16 (`pushOutOfBox`) faced every outfield keep-him player at the ball. That overwrote a placed outfield taker's heading: a penalty taker was 9.3 degrees off, and the corner and free kick passed only because their offsets point near the heading. The placed taker now keeps his look point (`HeldTakerLookAt`). This also corrects the corner's facing, which now is its heading.
- Wiring: `MatchSetup::pesDerivation` (required), `Match::GetPesDerivation`, `gfsim_run`, the gdext `PesLibrary::derivation()` (both changed objects compile), and `tests/sim/pes_match_constants.hpp`.
- Mirror: `Match::LeftFootedOf(taker->GetPlayerData())` (PlayerDB, on origin/main), which is `FUN_1406E6660(player, 0x34) == 1`.

## Stand-ins (ponytail)

- Card id 0 (`FUN_1406EA7B0`): GF has no signature cards. Stat 0 (`FUN_1406E81F0`, `FUN_1406EA760`): GF has no style record. Second kicker 255: GF has no second free-kick kicker.
- `FUN_14092E7E0`'s class and id gate (`FUN_140A60F00`, `FUN_140A610B0`), with its `FUN_140A43170` alternative yaw, is not ported. GF always takes the MODE bearing, as the corner port does.
- Physique 1.0 (player +0x3B18). The placer's height is not carried. The stand is set at prepare and not followed.
- Constants are literals with VA citations (the corner's precedent): 0.5 m (0x142565600), 1.5 m (0x14259BDDC), 90 and 270 (0x14259BE5C, 0x1425A2008), 360 (0x14259858C). Moving them to `PeImage::ReadVa` is a follow-up.

## Tests

- `tests/pes_derivation_test.cpp`, `RunUpWalks.*` (install-free, synthetic rows): the six mutations of `docs/re/pes-re-motion-remap.md` section 7, mapped below. Plus the install test above.
- `tests/sim/pes_setpiece_stand_test.cpp`, `SetKickStand.*`: the free kick and penalty are placed and held, at the contact offset's length from the spot. The free kick faces the contact yaw plus the goal bearing. The penalty faces the contact yaw plus 90 or 270. Pure tests cover the restart yaws, the contact yaw, the heading, and the free kick's pull-in and clamp.

| # | Mutation (in `derivation.cpp`) | Killed by |
|---|---|---|
| 1 | free kick gate dropped | `FreeKickWalkIsSkippedWhenASecondKickerIsSet` (and the stat test) |
| 2 | free kick stat not zeroed | `FreeKickStatIsZeroedWhenASecondKickerIsSet` |
| 3 | penalty gated by the second kicker | `ThePenaltyWalkIsNotGatedBySecondKicker` |
| 4 | card 0 matched by id | `AFreeKickTakerWithNoSecondKickerTakesTheFirstCardZeroRow`, `APenaltyTakerWithNoCardTakesTheFirstPenaltyRow`, `ThePenaltyWalkIsNotGatedBySecondKicker` |
| 5 | first match reversed (last wins) | the same three tests |
| 6 | stat walk matches sub, not sub minus one | `TheFreeKickStatWalkMatchesSubMinusOne` |

Each mutation was applied alone, the derivation test binary rebuilt, the run recorded, and the line restored. `derivation.cpp` matches the commit afterwards, and no `MUTANT` text remains. The red state: the tests were written before the functions, and the first build failed on the missing symbols. That failure is the red state I recorded, not a separate red run.

## Verification (merged tree `f3facbd`)

- `pes_derivation_test` 30/30. `pes_setpiece_stand_test` 19/19 (install present).
- Repro (`gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 5 --pes-loop --pes21-root <install>`, rebuilt in the same command):
  - origin/main `b7cd787`: `[determinism] d0748d444ae7e0e0`, 31803 ticks to full time.
  - `FKPenaltyPlace` `f3facbd`: `[determinism] 4fe5adc9ea791f06`, 27346 ticks to full time. Two runs gave the same hash. The longest stoppage is 23.1 s against the 32.5 s bound.
  - The hash change comes from the free kick and penalty takers now being placed and held at their stand points, where they were 2.3 m and 3.0 m stand-ins. The free kick clip is now 1261. The facing fix also moves corner takers. I did not run an ablation to split these.

## Open items

1. The corner's run-up cell: GF's corner reads (0x509, 9), which is 1248, while `docs/re/pes-re-kicker-standby.md` section 12 says the corner reads 0x50C. Not changed here.
2. `FUN_14092E7E0`'s class and id gate, and its `FUN_140A43170` yaw, are not ported (see Stand-ins).
3. Live check, read-only: `MI+0x1740 = 255` and the card 0 walk to 1261 at a free kick restart, for one default taker.
4. The stand-ins above: card, stat, second kicker, physique, height.
