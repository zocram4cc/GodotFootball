# BugFix: four decode-wave defects (red → green + mutations)

Branch `BugFix`, base `origin/main a17df0d`. SHAs (confirmed by `git ls-remote origin BugFix`):
- `c1aff07` four fixes (red tests, green code, mutations M1/M2b/M3 run and reverted)
- `7b5b949` clean cutover (class word to shared contract, foot band host-only)

VA check (§Inputs): every VA in the brief appears in its cited doc —
`140875580`/`140875a10` in `pes-re-kick.md`, `141e5ca20`/`1406c2f80`/`1406cbc10` in
`pes-re-a3helpers.md`, `22e4c` in `pes-re-teamtick.md`.

## 1. LateralDamping reads the wrong object → record by name, +0x20 block
- Red: `DampingRecordInstall.ReadsTheFreemoveBoundsFromThePlausibleBlock` failed on main
  (loader returned garbage-path floats; expected 42.0/18.0/69.5).
- Fix: `LoadDampingRecord` reads `freemove.o` by name as on disk from
  `common/match/constant/constant_player.bin` and returns the record's +0x20 floats
  {42.0, 18.0, 69.5} — the only plausible bounds block in the 80-byte object
  (`+0x08` reads ints 32/0/64 = denormals; `+0x30` has scale ≈ 0). The live path
  `FUN_141e5ca20(0x2e)+0x10..0x18` reads 9.3e-39/1.85e28/4.9e-44 on this build
  (addendum item 2), so no PES path to the bounds is known: carries a
  `// ponytail:` naming the open path. Garbage-path comment and +8 offsets deleted.
- Old install test replaced (it pinned the garbage path): new
  `ReadsTheFreemoveRecordByNameFromTheInstall` asserts loader == hand recompute.
- Mutation M1 (bounds back at +8): new test FAILED, reverted.

## 2. kFootBandSide = 0x7c as word index → class word 0x1f, renamed
- Red: `RunTurnRunSlipdown.SlipLimitReadsTheClassWordAtByte0x7c` sets word 0x1f = 3 and
  expects the 135° limit (12.5° damped); the old code read word 0x7c (byte 0x1f0 = 0)
  and took the 180° limit (13.33°). Failed on main, passes now.
- Fix: PES byte 0x7c = word 0x1f (addendum item 4: `FUN_1406c2f80` reads
  `*(int*)(req+0x7c)`, INIT writes 1). The word is the shared 0..3 class word, so it
  lives once in `movestep.hpp` as `kClassWord`/`kPrevClassWord`; group C `kBand`
  and group D `kClassWord` deleted (their rule: shared words move to the contract).
  Foot-window gates (`movestep_run.cpp` ×2) read the host foot band
  (`P+0xab8 vt+0x78`); stale request writes deleted (replay wrote `=1` over the class
  word). Step gates (`movestep_step.cpp`) and slip limit (`movestep_turn.cpp`) read
  `kClassWord`. No `kFootBandSide` remains anywhere.
- Mutation M2b (wrong word `kLiveKind` at the slip gate): red test FAILED, reverted.
  (M2 `<3`→`<2` passed — dead branch at class 2 for that test's angles, noted.)

## 3. NearestList own-team block includes the owner → self excluded
- Red: `NearestList.OwnersOwnTeamBlockExcludesTheOwner` (owner absent, own count 10,
  opponents still 11) failed on main; passes now.
- Fix: `BuildNearest` skips `j == i` in the owner's own-team block only
  (`pes-re-teamtick.md` §3.2, CONFIRMED live `rank[0] = 10`). Counts and ranks follow.
  Two pre-existing tests updated to the corrected contract (own count 10, owner rank absent);
  `RanksGive…` still checks all opponent ranks. `NearestOpponents` untouched.
- Mutation M3 (self included again): red test FAILED, reverted.
- Peer note: `agent://A4Walk` confirmed only the opponent block is read; no rebase needed.

## 4. Kick-chain comment corrected (comment only, no behavior)
- `ball.cpp`: `FUN_140874860` takes only `v_R` from `FUN_140875580` (ω discarded, §1.3–1.4);
  kick spin is 2π·v_R uncapped or `kicker+0x108 · FUN_140875a10(...)` capped at 8 rev/s
  (§1.5). Old comment (R → `FUN_140875170` → no clamp) replaced.

## Verification
- Binaries run (all with `PES21_ROOT=<workdrive>/PES21` where install-backed):
  `pes_movestep_test` 92/92, `sim/pes_offball_test` 53/53,
  `pes_movestep_turn_test` 43/43, `pes_movestep_run_test` 40/40,
  `pes_movestep_step_test` 13/13. Gate not run (Main gates).
- Coordination: `MoveStepCheck` (no collision, instrumentation reverted),
  `A6Whistle`/`A1Spin`/`A4Walk` (no shared files) messaged; A4Walk acknowledged.
- PII: no machine paths in changed files.

## Open items
1. `freemove.o` bounds: no PES path to the +0x20 block is known (ponytail in loader).
   Decode which pointer reaches the live `0x42a2bbd0` object and where its bound
   fields sit (addendum next step 2).
2. M2 `<3`→`<2` passed: class-2 coverage at the slip limit arm is thin (test uses
   classes 0 and 3). A class-2 case would pin the `==2 takes 135°` arm.
3. Smoke not run (per 16-10-26 split, Main runs it at batch merge).
