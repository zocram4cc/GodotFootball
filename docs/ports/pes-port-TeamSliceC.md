# pes-port-TeamSliceC: designation publisher, corner ordinal fill, lateral key (PES path)

Branch `TeamSliceC` (remote head `b15073e`). Base `origin/TeamSliceA` 24621ce. Worktree `<workdrive>/gf-TeamSliceC`.
Tags: CONFIRMED = raw disassembly, decompile or image bytes; INF = inference (reason given); OPEN = blocker named.

## Commits (all on the remote, confirmed by ls-remote)

| SHA | content |
|---|---|
| 706829e / 8d156ac | C1 red / green: FoldClass, BucketByClass |
| b0ef464 / 59b6e35 | C2 red / green (superseded: see Correction 1) |
| 85aff66 / 51efcc5 | C3 red / green: DesignateTwoCf |
| 115ec82 | insertion sort in place of std::stable_sort (superseded by 9f6c7a5) |
| 9f6c7a5 | publisher compaction fix, FillOrdinals (FUN_14051B8B0), install constants, tests |
| b15073e | wiring: UpdatePesDesignation in Team::Process (pes_loop), PesBoxMen for the corner box (pes_loop) |

Files: `src/sim/onthepitch/pesdesignation.{hpp,cpp}`, `pesteamconcepts.{hpp,cpp}` (constants field + loader call), `teamAIcontroller.{hpp,cpp}`, `team.cpp`, `tests/sim/pes_designation_test.cpp`.

## Tests and mutations

`pes_designation_test` (Release, PES21_ROOT=<workdrive>/PES21): 11 of 11 PASSED. Without PES21_ROOT: 9 pass, 2 install tests skip.
Tests: ClassesBucketed, LateralOrderWithinBucket (install), SingleMemberBucketEmitsItself, ConstantsLoadFromTheInstall (install), LateralKeyFollowsTeamDirection, BitFifteenMenFirstThenScore, DesignatedManIsExemptFromThePenalty, ClassPenaltiesAndBonusRankTheBox, RemainderReadsTheRecordTopDownWithoutElementZero, NoEligibleMenLeavesTheOrdinalsEmpty, PairArmedOnlyWhen87CD2.

Mutations, each rebuilt, run, reverted (the final run is green):
- M1 publisher sort comparator reversed: LateralOrderWithinBucket FAILS.
- M2 emission ignores DAT rank: LateralOrderWithinBucket FAILS.
- M3 stage one not recorded as box men: BitFifteenMenFirstThenScore FAILS.
- M4 class 2 scores its aerial value: ClassPenaltiesAndBonusRankTheBox FAILS.
- M5 remainder reads element 0: RemainderReadsTheRecordTopDownWithoutElementZero FAILS.
- M6 designated man not exempt: DesignatedManIsExemptFromThePenalty FAILS.
- M7 lateral ignores team direction: LateralKeyFollowsTeamDirection FAILS.
- C3 gate `< 2` (earlier step): PairArmedOnlyWhen87CD2 FAILS.

NOT covered by a test: the wiring itself (UpdatePesDesignation under pes_loop; PesBoxMen in PrepareSetPiece). See Open 1.

## Smokes (2 min, seed 7, teams.sql 1 v 2, --pes21-root; no lock)

- switch-off: `[determinism] 20f28a3df9b0ce12`, equal to the baseline. Exit 0.
- `--pes-loop`: `[determinism] 7b3252ff790975c9`, equal to the earlier baseline. Exit 0. The hash did NOT move. The publisher's only consumer is the corner box, and no corner counter exists in the log, so I cannot say whether a corner reached PesBoxMen (Open 2).

## Decoded (VAs)

1. Publisher FUN_14051E350 (CONFIRMED raw 0x14051E350-3FC): two calls to FUN_140A40CA0 (0x14051E380, 0x14051E3C2) with identical inputs (RCX = T+0xC48, R8D = side, R9B = 1). Each writes the 48-byte record `{count, slot[11]}` into P+0x7CDC (and a twin at P+0x7D0C). With P = T+0xC48 (FUN_14051C840 alias init), P+0x7CDC = T+0x8924: the same place community-qa F9 named. This clarifies the earlier "no +0x8924 destination" wording, which only held for direct E8 stores.
2. FUN_140A40CA0 (CONFIRMED raw 0x140A40CA0-0F67, decompile agrees): bucket build by folded class (0x140A40D47 jump table), then per bucket: the side block is COMPACTED to the bucket's members (only slot < 22 is kept), sorted by FUN_140A79640 (ascending key, stable), and the k-th emitted member is the sorted member at rank DAT[c][k] (0x140A40F20-F67: index 10c+k, element side*11+rank).
3. DAT_142643596 (CONFIRMED, image bytes at offset 0x2643596): row c at bytes 10c..10c+9. Rows 1..10 are permutations of 0..c-1 (checked by the loader). c=2 -> (1,0); c=3 -> (1,2,0); c=10 -> (5,4,6,3,7,2,8,1,9,0). Row 11 is zeros: a bucket of 11 members would repeat rank 0 (PES's behaviour, not reachable with real rosters).
4. FUN_14051B8B0 = the corner ordinal fill (CONFIRMED decompile + raw bytes). Reader of the table: the remainder fill reads P+0x7D08 down to P+0x7CE4 (elements 10..1, each only while index < P+0x7CDC count); element 0 (P+0x7CE0) is never read. FUN_1405FC860 does NOT read the table (grep of its decompile and disassembly finds no 0x7CDC/0x7D08 displacement; its only readers are the sheet takers and bit 15). Stages: (1) CPU: on-pitch men with bit 15 (FUN_140A64030(·,0xF)) in slot order at ordinals 10 down; human side: the sheet takers (FUN_1408C5060); (2) keeper chase (FUN_140509890) puts the keeper next; (3) eligible men by score, DESCENDING (comparator LAB_140602040 raw bytes via objdump: returns -1 when a.key > b.key; FUN_141E53540 keeps cmp<=0 on the left), stored then limited by min(eligible, T+0x6D0); (4) remainder from the record; (5) ordinal 0 = keeper unless placed.
5. Score (CONFIRMED raw 0x14051B8B0 L206-249): score = height(+0x31) + jump(+0x2c) + heading(+0x1d). Not the designated man: -100 (DAT_14259D674) for class 0 unless keeper-chase, -100 for class 1 always, 0 for class 2 and 10+, the score for classes 3..9. Classes 6..9 add +1000 (DAT_14256AE34). The designated man (P+0x3EF8) keeps the score and no penalty.
6. Constants (script-read from the image bytes, CONFIRMED): VA 0x14259D674 = bytes `00 00 c8 c2` = -100.0; VA 0x14256AE34 = bytes `00 00 7a 44` = 1000.0. Read at runtime by LoadConstants (PeImage::ReadVa), not typed.
7. Bit-15 test FUN_140A64030 -> FUN_1408CB0B0 (CONFIRMED decompile): record+0x48 bit 15 AND DAT_142613D20[class] bit 15 = pes::style::Gate(compat, class, StyleWord, 15), already ported (offball compat rows).
8. Restart men FUN_140A3C8F0 (CONFIRMED): ids at MI+0x173C and +0x1740 are excluded from stage two (not carried: no GF restart men at corner time).
9. Lateral key (item 4, see below).

## Item 4: lateral key sign per side

- CONFIRMED: the slot-point row writer FUN_1404A8620 (0x1404A87E9-0x1404A8892) copies the sheet triple FUN_1408C50E0(sheet, slot, 0, 3) into (S+0x3A9C+12i) with NO side factor. The key f2 sits at S+0x3AA4+12i (the `(slot%11+0x4E3)*12` load in FUN_140A40CA0). FUN_140A40CA0 reads f2 raw for both sides. So the sort key is the sheet's f2 for both sides, and the sign is whatever the sheet frame is.
- INF (high): the sheet frame is team-relative, world = dir·f2. Evidence: (a) choreography §11.5 near-post test `sign(base.z·dir)` compared with `sign(R.z)` uses f2 (T+0x46E4+12·slot+8 = f2) against the world restart point; (b) §11.4 kick-off kicker at `(-0.25·dir, 0.45·dir)` applies dir to z. So the stand-in is TeamFrameLateral(team->GetSide(), world z): GetSide() = -1 for team 0, +1 for team 1 (team.cpp:239-242).
- OPEN: no live verification (the read-only live read was not done; budget). The stand-in is the GF adapted formation point's coords[1] (ponytail, named at UpdatePesDesignation).

## Readers and migration

- Migrated: the corner BOX men of the attacking side (teamAIcontroller PrepareSetPiece, the `boxAttackers` pick). PES path: PesBoxMen = FUN_14051B8B0's stage one and two men in placement order; rank = placement order (the grid is the GF stand-in for positionCK). Switch-off keeps GF's aerial pick: the switch-off hash must not move, so the GF pick is still there, on the non-PES branch only. This is the pes_loop pattern used elsewhere; it is not the "delete the GF pick" of the brief, which conflicts with the switch-off hash rule.
- NOT migrated (OPEN): the defending post men (FUN_14057CBC0 near post, FUN_14057CF40 far post, choreography §11.5). They do not read the table. Their decoded rule differs from GF's pick: near post = min(height+jump+heading) over class 2..4 on the SAME side, else over class 2..6 on ANY side; far post only when attack level < 1, over role != 0 men not already POST_MAN. GF uses a per-player side filter instead of the two-stage fallback.

## Corrections

1. C2 model (commits 59b6e35, 115ec82): wrong. The sort included the 0xFF/0.0 padding, so a single member with a positive key emitted 0xFF. The sorter compacts each side's block first (FUN_140A40CA0 writes the counts at RSP+0xD0/0xD1 = the sorter's +0xB0/0xB1). Fixed in 9f6c7a5; the old expectation {0xFF,...} was wrong and is now {0,...}.
2. team-layer2 §1 ("flag 0 sorts the whole buffer") is refined: flag 0 sorts each side block separately (compacted), see decode 2.
3. community-qa F9 "+0x8924/+0x8928" was right in place (= P+0x7CDC/P+0x7CE0 with P = T+0xC48); the team-layer "no +0x8924/28/54 destination" line only meant no direct E8 store.

## Open items

1. Wiring test: no red test for UpdatePesDesignation (under pes_loop) or PesBoxMen (corner). A match-level test is needed (pattern: pes_defensive_shape_test's fixture, Match(..., Properties{pes_loop=true}), match->Process()). Not written.
2. Corner path coverage: the 2-min pes-loop smoke did not move the hash. Either no corner ran or the box men matched GF. A corner counter (or a 20-min smoke) is needed to say which.
3. Ponytails in FillOrdinals' inputs (named in teamAIcontroller.cpp PesBoxMen and in the commit): P+0x3EF8 designated man (FUN_1405FC860) = none; keeper chase FUN_140509890 = false; kind-8 exclusion FUN_140A63950 = none (table ctx+0x25CF8 writer OPEN); restart men FUN_140A3C8F0 = none (kickers chosen after this placement in GF); human side FUN_140A55CF0 = CPU (GF has no per-team human flag); eligibility bytes T+0x4C2C = on-pitch count (writer undecoded); gate FUN_140A533A0 = the GF corner; duplicate human takers placed once (PES stores twice).
4. Ties: PES's ranking is a quicksort (FUN_141E53540, unstable); ported as a stable sort.
5. 2-CF designator (C3): not wired. T+0x87CD (its gate) has no decoded writer, and no reader of T+0xB828/B82C/B830 was found (no disp scan run). DesignateTwoCf is a pure function with a test; nothing calls it.
6. Lateral sign: INF (high) on the team-frame reading; not verified live (see item 4).
7. Constants plumbing: the designation constants are a field of TeamConceptConstants, read by LoadTeamConceptConstants (pesteamconcepts.cpp). That is the one loader edit, approved by Main for this loader; the A4Lanes3-2 stock-read switch may touch the same function (merge conflict risk).
8. Switch-off GF pick stays (see Migrated).
