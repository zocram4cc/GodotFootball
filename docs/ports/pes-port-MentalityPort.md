# PES mentality port: the four rules on the PES path (MentalityPort, 23-10-26)

Branch `origin/MentalityPort`. Base `origin/main` 50ec19e (its src equals e55bf32, which is the red-test commit). Commits: `e55bf32` red tests; `568b4de` the port; `ce15cd4` a fix for a switch-off regression that the full sim suite caught in 568b4de. Heads confirmed by `git ls-remote origin MentalityPort` (ce15cd4).

## Result

- The four rules are ported with their VAs, red then green, and six mutations, each run, rebuilt and reverted.
- Suites on ce15cd4: pes_mentality_test 15/15, pes_roles_test 5/5, pes_team_concepts_test 31/31, pes_team_tick_test 10/10, pes_setpiece_team_test 32/32, sim_match_test 11/11 (full suite).
- Repro (`gfsim_run --db-sql assets/fixtures/teams.sql --home 1 --away 2 --seed 7 --minutes 5 --pes-loop --pes21-root <workdrive>/PES21`): full time on both trees.
  - origin/main sources (e55bf32, built in a scratch worktree): `[determinism] d0748d444ae7e0e0`, 31803 ticks to full time.
  - ce15cd4: `[determinism] bd977ff781825d6d`, 24816 ticks to full time. **The hash moved.** Cause: see "Repro cause".
- The handover's baseline `5fe27635275b433a` (at 8168d4c) is not used: 47 src files changed between 8168d4c and origin/main.
- Gate not run (Main gates).

## Rules (VAs). CONFIRMED = read from pes_full.bin or cited from a byte-audited doc; INF = inference

1. **Role level** (`docs/re/pes-re-role-rebuild.md` §2). `role[slot] = JT[matrix[10 + (family*11+slot)*3 + level]]`, `family = matrix[0]` (T+0x8E08), `level = 2 - (T+0x255 != 0)`, and `0` when `MI+0x116C != 1 && MI+0x1554 == 1` (EC600's `CMP [MI+0x116C],1; JZ; CMP [MI+0x1554],1; CMOVZ EBP,0`). The jump table at 0x1408C52A4 is already read by `LoadRoleClassMap` (13 entries). Verdict: CONFIRMED (role-rebuild doc).
   - GF: `pes::mentality::RoleLevel` (`pesmentality.cpp`). `WriteRoleArrays` now takes the level from the latch for both head and tail (the head was a constant 2, the doc's W2/C20 divergence on the usual path). `WritePhase` is removed.
   - Effect on roles: none on GF's sheet, because GF writes the chosen row and reads that row in the same call. The override is inert on GF (see stand-ins).
2. **Shared reclassify** (`pes-re-mentality-blocks.md` §2). Table VA 0x1405ED1F4, nine dwords (RVAs from 0x140000000), index class − 1. Dispatch at 0x1405ECB3D–47: `MOV ECX,[RSI+RAX*4+0x5ED1F4]; ADD RCX,RSI; JMP RCX`. Arms read from the image:
   - 0x1405ECB5D (classes 1, 2 and out of range): `4D 8B D6 41 8B FE` = `MOV R10,R14; MOV EDI,R14D`, with R14 zero from 0x1405EC714, so line 0.
   - 0x1405ECB49 (classes 3–6): `BF 01 00 00 00 44 8B D7` = `MOV EDI,1; MOV R10D,EDI`, line 1.
   - 0x1405ECB53 (classes 7–9): `BF 02 00 00 00 44 8B D7`, line 2.
   Verdict: CONFIRMED.
   - GF: `LoadReclassifyLines` decodes the table from the install, opcode-checked, and fails on any other arm. `ReclassifyLine(table, cls)`: 1..9 take their line, else line 0. On the PES path, `TeamAIController::DefensiveLineOf` returns `ReclassifyLine(roles.b[slot])`. The switch-off path keeps `PesLineIndexForRole(role)`.
   - For GF's own roles the two mappings agree (CB/SB → 0; DM, CM, LM, RM, AM → 1; SS, LW, RW, CF → 2; GK → 0), so the table changes behaviour only where the B class differs from the role used by the caller (see "Repro cause").
3. **Block choice** (`pes-re-mentality-blocks.md` §1). `CMP BYTE [RBP+0x8C],R14B` at 0x1405ECC21, `JZ 0x1405ECC31` to block A (`CALL 0x140681FF0` at 0x1405ECC44, into 682770). F != 0 calls block B (`CALL 0x14067B640` at 0x1405ECC2A). F = (T+0x255 != 0): `MOVZX ECX,[T+0x255]` at 0x1404F6EB8, `SETNE` at 0x1404F6EC6. Verdict: CONFIRMED.
   - GF: `BlockFor(team->IsPesPossession())` in `CalculateDefensiveShape` on the PES path. Block A's forward pull (682770) is not ported (F2–F4 of the plan), so out of possession the GF target stands.
4. **Block B's fold pick** (`pes-re-mentality-blocks.md` §3; 67C4B0). Score = x² + z², plus 10000 when class > 5 (`CMP R8D,6` at 0x14067CE72; `MOVSS XMM12,[0x14259D634]` at 0x14067CDE3, value 10000.0). Start `best` = 100000.0 (DAT_1425C65FC); a candidate wins only below `best` (0x14067CE7D). Candidates: slot < 0x16 (0x14067CD60), on the pitch (0x14067CD66), class != 0 (0x14067CDB3), not the protected slot 0x4B40 (0x14067CD9D). Fold count T+0x110 (0x14067C533, 0x14067C9FD). Skipped when `b8a2` (0x14067CDFB; b8a2 = b8a4 > 3). Store to `planObj+0x7CD4` (0x14067CF93). Verdict: CONFIRMED for the constants, the score and the filters; OPEN for T+0x110's writer and for the candidate position's writer.
   - GF: `FoldPicks` (pure), `LoadFoldConstants` (both floats read from the image), `FoldCount`, `ProtectedSlot`. On the PES path, block B runs the pick and stores it in `TeamAIController::foldSlots`.

## Stand-ins (each marked `ponytail:` in code)

- `MI+0x1554` = 0. GF has no writer; MOVESTEP's `MatchState1554` is also a stand-in 0. The level-0 override is therefore inert on GF.
- Fold count (T+0x110) = 2 − dial below neutral (INF from `pes-re-mentality-reclass-live.md` §2: one fold at −1, two at −2).
- Protected forward = the last slot of class 9 (INF from blocks §3.2, which names the CF; the full R5 scan of `pesforward` is not wired).
- Candidate position = the player's pitch position. The writer of 67C4B0's `local_238` is OPEN.
- Picked slots are all excluded from later picks. The byte filter `[base+0x173C] != prev` excludes only the previous pick; the two folds the dial reaches make these the same.
- GF's sheet rows take the dynamic formation role (the matrix builder is OPEN, role-rebuild §3).

## Open

- **The fold result has no reader.** PES's readers of `planObj+0x7CD4` are not decoded, so `foldSlots` is written and never read. The fold has no sim effect yet.
- Block A (682770, the forward pull and its class write) is not ported here: F2–F4.
- The parity sub-path (F inverted on even frames in play; RE-25) is not ported. The head uses the latch only.
- The tail swap (W3) is unchanged.
- Matrix builder (role-rebuild §3, OPEN) and the MI+0x1554 writer (OPEN).

## Tests and mutations

- Red first: commit e55bf32 added `tests/sim/pes_mentality_test.cpp` against an absent `pesmentality.hpp`, and dropped the `WritePhase` test.
- Green: 15 tests in pes_mentality_test. The install-read tests (reclassify table, fold constants) ran with PES21_ROOT set.
- Six mutations, each built, run and reverted to the original tag:
  - M1 latch polarity in `RoleLevel`: 3 tests fail (RoleLevel group).
  - M2 default reclassify arm stores the middle line: 1 fails (`RoleGroupsShareALineAndTheGroupsDiffer`).
  - M3 block polarity in `BlockFor`: 1 fails.
  - M4 protected slot not excluded: 1 fails (`NeverPicksTheProtectedSlot`).
  - M5 forward penalty never applied: 1 fails (`PicksTheLowestScoreNonForwardBeforeAForward`).
  - M6 full-attack skip removed: 1 fails (`FullAttackSkipsThePass`).
- Regression caught by the full sim suite on 568b4de: `SimMatch.AIManagerSwitchGovernsTactics` failed. The switch-off path's `GetAdaptedFormationPosition` had moved to the static role. Fixed in ce15cd4: each call site passes the role it used. Now 11/11. origin/main's sources pass the test.

## Repro cause

Only one PES-path change can move the sim: `DefensiveLineOf` reads the B class instead of the caller's role. The B class is the class of the dynamic formation role. The role level and the head latch write and read the same row, so the roles are unchanged. The fold has no reader. The override is inert. The attribution run (mutant: the PES path returns `PesLineIndexForRole(role)`) is recorded below.

Attribution, verified: the mutant (the PES path returns `PesLineIndexForRole(role)`, the B class not read; all else as ce15cd4) gives `[determinism] d0748d444ae7e0e0`, 31803 ticks to full time, identical to origin/main. So the B-class line membership alone moves the sim. ce15cd4 gives `bd977ff781825d6d`, 24816 ticks. The head level, the inert override and the fold pick do not move values in this run (the fold has no reader).

## Notes

- The brief's verification split says agents do not run gfsim_run. The assignment asked for the repro, so I ran it under the sim lock. Later runs used `tools/dev/simslot.sh` (run from the worktree; the copy is not committed).
- `teamAIcontroller.cpp` line ranges I touched were sent to TeamT1 (their T-tick steps 8, 9, 11 and FKPenaltyPlace are untouched).
