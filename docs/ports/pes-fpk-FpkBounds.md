# FpkBounds: fpk member past EOF

Branch `FpkBounds` at `87f3cba` (base `origin/main` 36108d0). Pushed; `git ls-remote origin FpkBounds` confirms `87f3cba351f0d5827c55606e29c4743a4ce80380`.

## Result: Done (PES behaviour INF)

## PES behaviour (INF)

Evidence on the install, not on the binary:

- `effect/#Win/effect_st069_nf.fpk`: 67,872 B (VFS `pesfmt_dump ls`, the same size from the cpk entry).
- Entry 4 `effect_config.xml`: offset 13,792 (0x35E0), size 54,093 (0xD34D), end 67,885. Overrun 13 B.
- The XML text ends at `</effect>` at 67,860..67,868 (0-based; the last 3 bytes of the file, 67,869..67,871, are 0x00). The entry claims 16 bytes past `</effect>`: 3 present as NUL, 13 absent.
- Sibling packs `st069_df` (1,184 B), `_dr` (4,320 B), `_nr` (4,320 B) have no overrun. `nf` is the only one.
- **Correction (round 2):** I earlier read `0x1403d2940` as a payload copy that could read past a pack buffer. That was wrong. Its only direct caller in the image is `0x1403d2b0d`, inside `FUN_1403d2a90` (`0x1403d2a90`). `FUN_1403d2a90` has five direct callers (`0x1403d2219`, `0x1403d22a5`, `0x1403d236e`, `0x1403d287e`, `0x1403d2a72`); all pass a string key (path name) with `strlen`-derived size, `param_5 = 0` except `0x1403d2a72` (via `0x1403d2a20`, the common_effect loader). With `param_5 == 0` `0x1403d2940` copies `param_3` name bytes, not file payload. So this resource layer does not read member payloads, and it does not show the pack buffer being over-read. Member payload reader: NOT FOUND (`foxfpk` is not a literal or u32 immediate in `pes_full.bin`). Verdict stays INF.

Verdict: **INF.** Clamped, skipped, and zero-padded are all unconfirmed. Zero-padded and clamped parse the one overrun pack identically (round 2 sweep), so this install cannot tell them apart. The clamp is kept because it is the only option that never reads past the buffer. Upgrade path: locate the fpk member reader (the `foxfpk` check is not a literal or immediate in `pes_full.bin`) and read how it sizes the copy.

## Changes

- `src/pes/fpk.cpp`: reject only a member that starts past EOF (was: member end past EOF). A member running past EOF is clamped to `blob.size() - offset`.
- `tests/pes_formats_test.cpp`: synthetic `foxfpk` builder; `Fpk.LastMemberRunningPastEofIsClampedAndThePackStillOpens`; `Fpk.MemberStartingPastEofIsStillRejected`; install `FpkInstall.St069EffectPackOpens`. No `tests/pes_fpk_test.cpp` existed, so these go in the format file that already holds the fpk users.
- `tests/pes_effects_test.cpp`: `EveryCanonicalEffectConfigInTheInstallParses` asserts every canonical pack opens (was: report and skip).

## Tests and red/green

Red (on base + tests, unfixed `fpk.cpp`), `PES21_ROOT=<workdrive>/PES21`:
- `Fpk.LastMemberRunningPastEofIsClampedAndThePackStillOpens`: FAILED, Parse rejects.
- `FpkInstall.St069EffectPackOpens`: FAILED, Parse rejects.
- `PesEffects.EveryCanonicalEffectConfigInTheInstallParses`: FAILED at the st069 assert.
- `Fpk.MemberStartingPastEofIsStillRejected`: passes red, as a guard for the second branch.

Green (fixed): `pes_formats_test` 24/24 passed; `pes_effects_test` 7/7 passed. The effects test includes st069 with no NOTE.

Mutation (edit tool only; one `sed` used early was discarded and the file restored from a saved copy):
- M1, clamp removed: `Fpk.LastMemberRunning...` FAILED; `FpkInstall.St069...` FAILED on `EXPECT_LE` (member reads past EOF). `PesEffects.EveryCanonical...` **still passed**: its parser stops at `</effect>` before the 13 bytes past the buffer are used. The install invariant was added because of this.
- M2, rejection branch removed (offset check): `Fpk.MemberStartingPastEofIsStillRejected` FAILED. Others unaffected.
- Restored fix: diff against the saved fixed copy is identical.

## Not run

- `ctest` gate and `gfsim_run`: not run (brief: main runs them). `pes_effects_test` and `pes_formats_test` were run directly as instructed.
- `tools/check_pii.sh` not run (worktree). Changed files grepped for machine paths: none.
