# Q1: PES attack-level quota (FUN_140500200) and its ahead count (FUN_140A4F230), ported

Branch `MentalityQ1`, pushed at f3b7751 (confirmed by `git ls-remote origin MentalityQ1`). Commits: ffb0747 (red test, named constants, install pins), f7c9a6b (fix), f3b7751 (merge of origin/main 59deb16). Base 84018a6. Files: `src/sim/onthepitch/pesoffball.cpp`, `src/sim/onthepitch/pesoffball.hpp`, `tests/sim/pes_offball_test.cpp`.

## Result

- **Real port bug, fixed.** FUN_140A4F230 (the ahead count, also the quota gate) takes its offside line from the opponents' S1 list, not the attackers' own list. GF read its own team. Level 2 with four men ahead now runs the quota (PES) instead of admitting every slot (old GF).
- **Constants named with VA and read site.** Values unchanged; each is pinned to `PES2021.exe` by `OffBallQuotaInstall`.
- **Non-forward key is the S1 list float** (`|s·xg − x|`), written explicitly instead of the INF stand-in `fieldHalf − s·x` (equal on the pitch).

## Verdicts on the disputed points

Columns: point · verdict · bytes (VA) · encoded?

- **C4, forwards count toward the quota (first `need` sorted entries). CONFIRMED.** Forward path: `CMP EDX,7` at 0x140500663, stamp `MOV dword [..],0xC2C80000` (−100.0) at 0x140500693, `BTS` at 0x1405006A2. The final loop (0x1405007D8–0x14050088E) takes entries 0..need−1 of the team block (`CMP R8B,R12B` at 0x14050086E). The doc's "top 2 non-forwards" (doc §4) is refuted. Encoded (unchanged). Pinned by `ForwardsAdmittedAndCountTowardNeed`.
- **C5, gate: max(FUN_140A4F230 count, front-first beyond) < need → every slot. CONFIRMED.** 0x1405003D8–0x1405003E6 (`CMP CL,R12B; CMOVC` = max), 0x1405003ED `CMP DL,[RSP+0x20]`, 0x1405003F1 `JNC` to the quota, else 0x1405003F3–0x1405003FD sets all 11 bits. Encoded. Pinned by `AheadGateAdmitsEveryone`.
- **C6, FUN_1405AAF00 permit (class > 1 admitted, quota skipped). CONFIRMED in shape, NOT ported (Q4).** `TEST AL,AL` at 0x140500262; the store `MOV [R8+0xB8B8],EDI` at 0x1405002BE. The predicate is consulted only for L = 1..3: `JLE` at 0x140500251 skips it for L ≤ 0. Stub stays (ponytail, RE-7).
- **C7, level 0 → forwards only. CONFIRMED.** Need 0; the forward path still admits. Pinned by `QuotaLadderByLevel`.
- **C13, the class gate reads B. CONFIRMED.** 0x14050064F–0x140500663 read `[T+0x4C00+4·slot]`. GF's `roleClass` is `RoleClassOfSlot` = `roles.b` (teamAIcontroller.cpp:2426–2428).
- **Ladder need 0/2/4/6. CONFIRMED.** Immediates at 0x140500321 (XOR → 0), 0x14050031A (2), 0x140500313 (4), 0x14050030C (6); the plan's VAs are the instruction starts. Opcode and immediate bytes pinned by `OffBallQuotaInstall`.
- **Level ≥ 4 → every slot. CONFIRMED.** `CMP EBX,4` is at **0x140500232**; the plan's 0x140500235 is the `JL` after it. Plan §0 should read 0x140500232.
- **Near term. CONFIRMED.** 10.0 at 0x142588210 (0x140500726), 25.0 at 0x14259BE20 (0x140500732), divisor −15.0 at 0x14259D668 (0x140500741), ×5.0 at 0x14259222C (0x140500746). Side −3.0 at 0x14259D654 (0x14050078E) when the man's z-side equals the receiver's aim side and the receiver turns.
- **RE-14, score source of the quota list. ANSWERED.** Non-forward candidate value is the list float at 0x1405006DF (`MOVSS XMM6,[RDX+RCX·8+4]`). The list is FUN_140A714E0 (0x140A714E0): MI+0x22B9C for DL == 1, else MI+0x229B4. Formula `|±xg − x|` is `pesscorerows.hpp:72` (GF's port of the row builder). **INF:** the PES builder is not decoded this pass. Read site CONFIRMED.
- **RE-14, sort. PARTLY.** Quota sort is `CALL 0x140A79640` (0x1405007DF, DL = 0) → merge sort 0x140A79440/0x140A79240. The SortedRow contract says ascending by value. The comparator body (0x140A79050, 0x140A788C0) and tie stability are not decoded: **INF** (GF uses `stable_sort` ascending).
- **RE-15, FUN_140A4F230 vs GF `AttackersAhead`. NOT EQUAL; three parts fixed, one open.**
  - (a) **Line block. FIXED.** PES reads block `1 − t`: `RDI = (T0.poss ≠ 0)` (0x140A4F2CB–0x140A4F30C), i.e. the opponents. GF read its own team.
  - (b) **Sort sign. FIXED.** PES sorts that block by the attacker's sign, `DL = R12B = T(t').254` (0x140A4F2F8, 0x140A4F310). GF sorted by the opponents' own sign, which reverses the order.
  - (c) **Owner fallback. OPEN (ponytail).** PES: carrier, else `MA+0x2483C` (0x140A4F2A3–0x140A4F2AD). GF: carrier, else `receiver[t]`. RunWorld has no holder field, and BuildRunWorld is a busy file.
  - (d) **Constants. CONFIRMED equal.** K = 1.5 (0x14259BDDC, read 0x140A4F2E5); back clamp 7.0 (0x14259BDF0, 0x140A4F39E); window 5.0 (0x14259222C, 0x140A4F3D2); back window 12.0 (0x14259BE04, 0x140A4F3DB); lateral 52.0 (0x1425CDAA0, 0x140A4F3E4).
  - (e) **Count conditions. CONFIRMED equal.** (A ∨ B) ∧ C ∧ D, as in GF.
- **`pesPenaltyAreaLineX` (RE-15 value). Offset CONFIRMED, value INF.** The beyond threshold is `*(G+0x2AA8)+0x18` (0x140500361–0x140500368). The pitch marks F0..F8 sit at +0..+0x20; +0x18 is F6, which GF names `pesPenaltyAreaLineX` = 36.14 (gametypes.hpp:85). The runtime value is not read live.
- **C9, turning. CONFIRMED as PES, NOT changed here (Q3, RE-9).** PES: `|team+0x250 − receiver heading| > 15` (0x1405004F0 against 0x14259BE0C = 15.0; heading via 0x1404AB8E0 and 0x14043A0A0). GF still uses velocity against facing (ponytail).

## Blast radius (E8 scan of pes_full.bin)

FUN_140A4F230 callers: 0x140500341 (the quota, ported), 0x140607143 (the Chance reach and rank code that GF's `RankAngleOk` and `ChanceMaskReject` port), and six unported features (0x140580cc9, 0x1405aa5bf, 0x14060f07f, 0x14061755e, 0x14066c51d, 0x140a540d7). The fix reaches the same PES consumers that GF already ports.

## Plan corrections

- Plan §5 says Q1 has "RE prerequisite: none (CONFIRMED)", while §6 lists RE-14 and RE-15 as Q1 blockers. Both are answered above.
- Plan §0: `CMP EBX,0x4` is at 0x140500232, not 0x140500235 (the `JL`).
- Doc `pes-re-attack-level-pull.md` §4 ("top 2 non-forwards") is refuted by C4.

## Tests

Red first (on unmodified code, commit ffb0747):
- `OffBallMask.AheadCountUsesTheOpponentsLine`: `AttackSlotMask(w,0,2)` = **2047**, expected **29** (ids 0,2,3,4). Red.

Green after the fix (commit f7c9a6b): `OffBallMask.*` (17) and `OffBallQuotaInstall.QuotaConstantsAreTheExesOwn` (install ran, PES21_ROOT set). Full binary: **71 passed**.

Pins (already green on main, each mutation-checked):
- `QuotaLadderByLevel` (need 2/4/6, level 0 → none, level 4 → all)
- `ForwardsAdmittedAndCountTowardNeed`
- `AheadGateAdmitsEveryone`
- `FalloffAndSideAreImageConstants` (the 5 m bonus gap)

Mutations (applied to `pesoffball.cpp`, rebuilt, run, reverted to the committed text):
1. need(3) = 5 → `QuotaLadderByLevel` FAILED (only).
2. forward BTS removed → `ForwardsAdmittedAndCountTowardNeed` FAILED (only).
3. ahead gate off → `AheadGateAdmitsEveryone` FAILED, and the existing `AttackSlotsOpenToAllUntilEnoughAttackersAreForward` FAILED (it also pins the gate at level 2).
4. near bonus 5 → 3 → `FalloffAndSideAreImageConstants` FAILED (only).
5. line block back to the attackers' own list → `AheadCountUsesTheOpponentsLine` FAILED (2047 vs 29).

Merged tree (f3b7751): 52 gfsim-linked test targets plus `gfsim_run` built (`BUILD_ALL_EXIT 0`). Run directly, not under gf-sim.lock (verification split):
- 50 binaries PASSED, 999 tests in total.
- `pes_restart_delay_test` and `sim_match_test` hit my 300 s cap with no summary and no failure line in their partial logs. The first is the long restart test; the second is a whole-match test with its own 1800 s timeout. Neither was run to completion (no 20-minute runs). Their partial logs show origin/main's new `[stalemate]` diagnostics; not triaged here.

## Smoke (`gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 2 --pes21-root <install>`; not under gf-sim.lock)

- Default: `[determinism] 961c600e97796617`, 33164 ticks to full time (cap 194400), rc 0. Rerun: same hash.
- `--pes-loop`: `[determinism] d5bcdf7b73938eaa`, 46308 ticks, rc 0. Rerun: same hash.
- Versus main: not measured. No 2-minute seed-7 hash for origin/main is recorded (the 20-minute hashes in `tasks/15-10-26.md` are from older commits). The hash is expected to move: the line block changes run selection.

## Open (numbered)

1. Turning quantity (C9, RE-9, Q3): GF velocity against facing; PES heading test. Not touched in Q1.
2. Owner fallback in FUN_140A4F230: `MA+0x2483C` versus GF `receiver[t]`. Needs a holder field from BuildRunWorld (busy file).
3. FUN_1405AAF00 permit (C6, RE-7, Q4): stub stays false.
4. S1 list builder formula (`|±xg − x|`, pesscorerows port) and the quota sort comparator and tie order (0x140A79050, 0x140A788C0): INF, not decoded.
5. Runtime value of pitch mark F6 at G+0x2AA8+0x18: not read live; GF constant 36.14.
6. The two timed-out binaries (restart delay, whole match): partial logs clean, not completed.
7. Plan §0 citation 0x140500235 should read 0x140500232.
8. Stalemate lines in restart and match logs (origin/main diagnostics): not triaged.
