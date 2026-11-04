# DribbleArms2: the two seeded tests re-staged by census, the smoke move explained by ablation, fast gate (DribbleArms2, 03-11-26)

Branch `DribbleProposal`, worktree `gf-DribbleProposal`, scratch `gf-DribbleArms2-scratch` (binaries, logs; not committed). Continues `pes-port-DribbleArms.md` section 5-6 (Left 1 and 6 ablation). All numbers below are runs of this session (CONFIRMED).

## 1. Merge and push
`git merge origin/main` twice (`0d0e4d1d`, then `ab538b9c` after Main's ccache change `d250bfd5`); pushed, `ls-remote` shows `ab538b9c...` for `DribbleProposal`.

## 2. Re-staged seeded tests (commit `7757d317`, assertions unchanged)
Method: the test itself is the census. A temporary env override (`GF_CENSUS_SEED`, removed before the commit) ran the real test on one seed per process, serial lanes through `simslot.sh` (4 lanes, each `timeout 1500`), stopping once enough seeds passed (ceremony: 3, red card: 1). A PASS means the test's own assertions held on that seed.

| Test | Old seeds | Census result | New |
|---|---|---|---|
| `PesThrowInRelease.ACeremonyThrowInIsReleasedFromTheHandOverTheSpot` (`kCeremonySeeds`) | 5, 7, 9 | no ceremony (path B) throw-in in 1-16, 19, 21, 23, 25, 27; ceremony in **17, 18, 29** | `{17, 18, 29}` |
| `PesRestartDelay` ceremony test (its own `kCeremonySeeds`) | 5, 7, 9 | same matches (a 20-minute run stops at the first ceremony, which lies in the first two minutes of seed 17) | `{17, 18, 29}` |
| `PesForfeitMatch.ARedCardRemovesTheOffenderAndTheSideIsNotForfeited` | `SEED` = 7 | seeds **1 and 2** reach full time inside the 15-minute window | new `kRedCardSeed = 1`, used only by this test (`BuildMatch(minutes, seed = SEED)`); the other forfeit tests keep seed 7 |

Direct runs on the merged tree (Release, `simslot`): `pes_forfeit_test` 6/6 (red-card test 178 s), `pes_restart_delay_test` ceremony test PASSED, `pes_throwin_release_test` first five tests OK incl. the ceremony test (175 s); the sixth (`TheLiftTurnsACarriedThrowInTakerToFaceThePitch`) was cut by my 1500 s wrapper timeout (cumulative), left to the gate.

## 3. Smoke move: ablation (seed 7, 2 min, gate flags, `simslot`)
Tree: the branch head with `git revert --no-commit 6e851f03` (the deletion commit), then single pieces forced; each a rebuilt `gfsim_run`. Restored afterwards (`git checkout HEAD -- src tests/...`, mtimes new, rebuilt).

| Tree | switch-off | `--pes-loop` |
|---|---|---|
| main baseline (brief, after batch AV) | `3b73c968b4a377e0` | `0e480b158cf80bd4` |
| T1: deletion commit fully reverted | `3b73c968b4a377e0` | `0e480b158cf80bd4` (= main exactly) |
| T1 + COM dribble knobs deleted only (`AIfunctions.cpp` from HEAD) | `9adb2fd686832bdb` | `0e480b158cf80bd4` |
| T1 + COM inference (`InferCom`) off only | `5000988f0ca992f5` | `e61f4999c3ab79ad` |
| T1 + style inference (`InferPlayer`) off only (COM still inferred from it) | `eb93efbc3e23d67c` | `ea0c96891347e0d9` |
| T1 + both inferences off, knobs still in (mask 0) | `7e371da577eeb18d` | `827a3deecedd8d93` (= the branch's, bit for bit) |
| branch (deletions in) | `7e371da577eeb18d` | `827a3deecedd8d93` |

Reading:
- The revert reproduces main's hashes, so the move is the deletion commit and nothing from the merged-in main work.
- **Knob deletion** moves only switch-off (`AI_GetBestDribbleMovement` is the GF path; `--pes-loop` does not call it): with the inferred COM masks the invented factors (1.3, 1.15, 0.6, 0.75, 0.5-blend) bent the drive. With an empty mask they return the base values, so on the final tree it is neutral.
- **COM inference** and **style inference** each move both modes: COM masks gated the carrier planners and `PlayerSkills::Unlocks`; the style word gated the keeper (bits 19/20), Destroyer (12), Cross Specialist (7), `StyleLb` and run-depth terms.
- Both off gives the branch's hashes exactly, so the Player.bin `+0x150` writer and the renames move nothing here (the fixture/placeholder records have the flag clear).
- Direction: a fixture player has no row, so PES's default row (style word 0, COM 0, `FUN_140AA1700` zeroes the live record) is what the deleted GF stand-ins replaced. A PES-fidelity move, allowed by the task.

## 4. Gate
(see section 5, filled at the end)

## 5. Left
1. Items 2-5 of `pes-port-DribbleArms.md` section 6 are untouched (Player.bin -> row path for no-save players, `FUN_140922DD0` neighbours, real-call cases for `0x142130EE0`, COM source for the carrier planners).
