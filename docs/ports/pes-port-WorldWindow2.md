# PES Clear chain on GF state: merge, angle oracle, record map (WorldWindow2, 28-10-26)

Branch `EndClear`, worktree `<workdrive>/gf-EndClear`. Pushed head **`a769434c`** (`git ls-remote origin EndClear` = `a769434c628aa30ab283ccdfa50125c574c11383`, confirmed). Tags: CONFIRMED = read in tool output this session; INF = inference; OPEN = not settled.

## 0. Verdict

- Step 1 (merge, push): DONE. Pushed `d5b5e1e4`.
- Step 2 (commit the oracle test): DONE. Pushed `a769434c`. Green, with the binding set corrected (§2).
- Step 3 (GF state in PES layout for the angle and Clear reads): NOT DONE. The angle's on-path reads are now mapped by byte (§3). No GF writer was identified for any of them, so nothing was moved into the window.
- Step 4 (danger writer `0x140699960` / builder `0x142119390`, delete stand-ins): NOT DONE. Blocked on step 3: the writer reads the same unowned world state, and no capture has danger = 1.
- Step 5 (FillUnitTable kind 1 to the translated Clear unit, `forcedClear`, stand-in deletion): NOT DONE. Blocked on steps 3–4. No red test, no green, no mutation for steps 3–5.
- No GF stand-in was touched. `elizacontroller.cpp:2774` and `:2951` remain.

## 1. Git

- `git merge origin/main` (`cd33cbde`) and `git merge origin/SpecGen` (`d5b5e1e4`) landed clean.
- Symlink trap (CONFIRMED): plain `git fetch`/`merge`/`push` fail on `extern/godot-cpp`. Working set: `git -c submodule.recurse=false -c status.submodule=none -c diff.ignoreSubmodules=all -c fetch.recurseSubmodules=false -c push.recurseSubmodules=no`, with the identity `-c user.name=... -c user.email=...` placed before the subcommand. Identity flags after `merge` are rejected.
- Tool note (CONFIRMED): the edit tool resolves relative paths against the predecessor repo `<local>/Code/GameplayFootball`. Use absolute paths into the worktree.
- Build note (CONFIRMED): after the SpecGen merge (`pes_state.hpp` changed) `pestranslated` rebuilt in full, 2533 s at `-j3`. Later incremental builds are short.

## 2. Angle oracle test (step 2): committed as `a769434c`

File `tests/pes_angle_oracle_test.cpp`, registered in `tests/CMakeLists.txt` against `pestranslated`. Skips unless `GF_PES_ANGLE_ORACLE` names a PES-derived directory (not committed). Oracle directory: `gf-EndClear-scratch/oracle/seq021` (capture `live2/pid407792/seq021`).

- `CapturedCallsMatchThePesReferenceBitForBit`: PASS (1 case). `pes_fn_142108630` on the capture's memory returns PES's rax, xmm0 and write log bit for bit (CONFIRMED).
- `EachRecordBindingChangesTheResult`: PASS (7 flips).

Correction to the draft (CONFIRMED by a scratch byte scan, `gf-EndClear-scratch/probe/dep_scan.cpp`, every byte of each region XOR 0xFF, result compared):

- The draft's bindings `G+0x40 +0x2920`, `MI +0x1168` and `ctx+0x1900` (E0 count) did NOT change the result. They are not read on this call's path.
- The test now flips on-path bytes: `G+0x40 +0x1C`, `G+0x40 +0x80`, `MI +0x1198`, `G+0x360 +0x8`, `G+0x2AA8 +0x0`, `ctx+0x18 record +0x2482C`, `ctx+0xE0` (ring word 0). All seven change the result.

Mutation check: each of the seven flips is the mutation for its binding (flip, rerun, expect a change). The scan's before/after is the mutation record: 17 changed bytes in G+0x40, 4 in MI, 4 in G+0x360, 9 in G+0x2AA8, 12 in ctx+0x18, 8 in ctx. The scratch source was reverted; the committed test is the only copy.

## 3. On-path map of the angle's world reads (CONFIRMED for seq021; one capture, so a lower bound)

| Record (PES access) | On-path bytes (scan) | GF state today | Notes |
|---|---|---|---|
| G+0x40 carrier record | +0x1C..+0x1F, +0x24..+0x27, +0x80..+0x8B, +0x90..+0x93 | none (not in `PesWorld`) | +0x2920 and +0x4/+0x4C are read but do not change this result. Identity OPEN. |
| MI (G+0x270) | +0x1198..+0x119B | `pesclock.hpp:159` names it `frame` (MI+0x1198, "the frame the judge runs on") | +0x1168 (period) is not on path here. Named, not in the window. |
| G+0x360 record | +0x8..+0xB | none | OPEN |
| G+0x2AA8 record | +0x0..+0x7, +0x17 | none | INF lead: `penaltyshootoutcontroller.cpp:35` reads `*(G)+0x2aa8` +0x8/+0xC as the goal's half width/height. The angle reads +0x0, not those. |
| ctx+0x18 record | +0x2482C..+0x24837 | none | OPEN |
| ctx (A0 = unit+8) | +0xE0..+0xE7 (ring word 0) | `pesevents.hpp` ring is E-store `E + r*0x1908`; ctx+0xE0 = E is INF | ring count +0x1900 is not on path in this call. |

Consequences (CONFIRMED by code reading of the window and the header):
- `PesWorld` (`pesworldrecord.hpp`) is one window at `kWorldWindowVa` (0x2000100000). The angle loads G from the image global `0x1436F3DC8`, and `PesMem` has no image-global segment for it. Its `rd` returns baked image bytes, not GF's G. Mapping G therefore needs a `PesMem` segment at `0x1436F3DC8`, plus the ctx and record regions in the window.
- GF has no writer for any on-path field except `frame`, whose GF writer is not yet traced.

## 4. What is needed for steps 3–5 (numbered)

1. Writers of G+0x40 (+0x1C, +0x80), G+0x360 (+0x8), G+0x2AA8 (+0x0) and ctx+0x18 (+0x2482C): PES VA, found by Ghidra xref or a targeted hook. The Ghidra GUI bridge is down; use `dec.sh` for decompiles. Xref search is not available through `dec.sh`. Hooks are Main's (`hooks.cfg`).
2. Trace `frame` (MI+0x1198) to its GF writer in `pesclock.hpp` and check it against the PES writer.
3. Ctx mapping: decide whether ctx+0xE0 is the E-store ring (INF). Compare ctx+0xE0 bytes against E's ring 0 in the same capture.
4. Map the rest of G+0x40 (a second capture or a wider scan of other calls, since this scan covers one capture).
5. Build the window: a `PesMem` segment for the G global at `0x1436F3DC8`, and the record regions in the `PesWorld` tail. GF writes each at its PES-writer site, with a `// PES writer: 0x…` citation.
6. Only then: translate or port the danger writer `0x140699960` and builder `0x142119390` (closure in `specs/closure-endclear`), and delete the `elizacontroller.cpp:2774` and `:2951` stand-ins.
7. Danger = 1 has no real call in the captures. Main needs a sampler on the Clear unit for the danger-1 arm (`carrier_clear_2109EB0` is `FIRST 1 EVERY 3000`, danger 0 in all five). Without it, the danger-1 arm cannot be proven.
8. Step 5 after 6: `FillUnitTable` kind 1 with `{type 5, a = angle}`, `forcedClear`, mutation per binding, and deletion of the stand-ins in the same change.

## 5. Counts

- `pes_*` binaries: 35 built (34 `pes_*` targets plus the new `pes_angle_oracle_test`). Run with `PES21_ROOT=<workdrive>/PES21` and `GF_PES_ANGLE_ORACLE` set, each under `tools/dev/simslot.sh timeout 600`:
  **TOTAL binaries=35, passed=760, skipped=3, failed=0, all rc 0.** Skips: `pes_formats_test` 1, `pes_editsave_test` 1, `pes_stadium_test` 1 (existing GTEST_SKIPs). The earlier 758-passed baseline (EndClear3, 34 binaries) plus the 2 oracle tests gives 760.
- Gate: not run (Main gates).
- Smoke: not run.

## 6. Open items

1. Steps 3–5 as in §4. Not closed in this session.
2. The scan is one capture (seq021). Other angle captures (seq028, 029, 032, 034, 035) may read more bytes. Run `dep_scan` against each.
3. `dep_scan.cpp` lives in `gf-EndClear-scratch/probe/`, not in the repo. Move it to `tools/dev/translate/` if it is kept.
4. The committed oracle test needs `GF_PES_ANGLE_ORACLE` to do anything. Main's gate does not set it; without it ctest reports two skips.
