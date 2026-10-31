# pes-port-VfsRoots: the default VFS mount is stock PES; sider's cpk.root folders and Lua modules are opt-in (owner ruling 30-10-26)

Branch `VfsOrder` (worktree `gf-VfsOrder`). Head `e3802b89`, confirmed by `git ls-remote origin VfsOrder` after the last push.

## Result

- `PesVfs::Mount(root, &err)` mounts the game's own cpks only: `Data/` then `download/`, download over Data. No sider `cpk.root` folder and no Lua module.
- `VfsOptions::sider` (`--sider` on `gfsim_run` and `pesfmt_dump`) turns the sider model on as before: Lua modules first, then `cpk.root` folders.
- `VfsOptions::gameplayMod` is a sider selection. Without `sider`, `Mount` returns an error that names sider; a named mod is never ignored silently.
- `origin/main` (15cc00c5, batch AQ and later) merged first; no conflicts.

## Commits (all pushed)

| step | SHA | content |
|---|---|---|
| merge | 7034ca75 | `origin/main` 15cc00c5 into VfsOrder |
| red | c35abc70 | four red tests on 7034ca75 |
| green | 41723be3 | `VfsOptions::sider`; gate in `Mount`; named mod refused without sider; `--sider` on gfsim_run and pesfmt_dump; explicit tests opt in |
| docs | e3802b89 | port doc `pes-port-VfsOrder.md` §10 |

## Tests

Red first (c35abc70 on 7034ca75): 4 of 17 fail, each for the expected reason.
- `VfsOrder.DefaultMountDoesNotServeASiderRootFile`: `Exists(constant_player.bin)` is true, served from a `cpk.root` folder.
- `VfsOrder.DefaultMountAnswersNoLuaModule`: `netblock.lua` answers for `content/netblock`.
- `VfsOrder.NamedGameplayModNeedsSider`: `Mount` accepted `{.gameplayMod = "modA"}` without sider.
- `VfsOrderInstall.DefaultMountServesTheBallFromTheCpks`: the default served sider's `livecpk\ball-root` ball bytes, not a cpk copy.

Green (41723be3): `pes_vfs_order_test` 18/18, no skips, `PES21_ROOT` set.
- The 13 existing tests stay; those that select a mod or read sider roots now pass `.sider = true`. `DefaultMountIgnoresTheBootFavorite` is renamed `SiderWithoutAModIgnoresTheBootFavorite` (it reads sider now).
- New, green: `VfsOrderInstall.SiderOptInServesTheSiderBall` (the explicit opt-in still serves the sider ball).

Other binaries on 41723be3, `PES21_ROOT` set: `pes_movestep_test` 114/114, `pes_uniform_test` 36/36, `pes_camera_test` 17/17.

Mutation (one): `if (!options.sider)` in `vfs.cpp` replaced by `if (false)`, so sider is always mounted (the old default). Result: 3 fail, 15 pass.
- Fail: `DefaultMountDoesNotServeASiderRootFile`, `DefaultMountAnswersNoLuaModule`, `DefaultMountServesTheBallFromTheCpks`.
- `NamedGameplayModNeedsSider` stays green: its refusal is a separate check, not the gate.
- Reverted: `vfs.cpp` identical to 41723be3; 18/18 again.

## Winners that change on this install

Method. `pesfmt_dump ls ""` with and without `--sider` lists the same 121075 paths, so sider only overrides; it never adds or hides a file. Sider's mount report on this install is `livecpk/ball-root` 7 files, `GameplayLoader.lua` "No Mod selected", `netblock.lua` 1 file, no UNMODELLED module. So the sider-changed set is exactly the 8 files below. Hashes are `pesfmt_dump cat PATH` output with `--sider` (before) and without (after), md5 prefix. "Before" is the 7034ca75 default (sider roots plus modules, No Mod).

| file | before (sider) | after (stock) | read by GF |
|---|---|---|---|
| `Asset/model/ball/ball021/#Win/ball.fpk` | 44816 d6f4350aff3a | 100800 7932d0a1f29f | only when a match asks for ball 21 (`match.gd`: `ball%03d`; `DEFAULT_BALL_ID` = 1) |
| `Asset/model/ball/ball021/#Win/ball.fpkd` | 48 dfcad71e4692 | 48 dfcad71e4692 | same bytes |
| `Asset/model/ball/ball021/#windx11/ball021_c_bsm_alp.ftex` | 349480 3913673576bc | 607856 cbb95e551a60 | ball 21's textures (INF) |
| `Asset/model/ball/ball021/#windx11/ball021_n_nrm.ftex` | 264936 a9956bc5b1d0 | 708784 a4c5d38eae6a | ball 21's textures (INF) |
| `Asset/model/ball/ball021/#windx11/ball021_s_srm.ftex` | 30248 16420c00df29 | 296352 05a34b8a8c98 | ball 21's textures (INF) |
| `common/menu/general/topModeSelectDMM.bin` | 760296 2a4d7ed75907 | 762517 f9dca18f834e | no reader (grep of src, godot, apps, tests) |
| `common/render/thumbnail/ball/ball_021.dds` | 87536 e587447c6300 | 87536 f4000e3a3c43 | no reader |
| `common/script/flow/Exhibition/ExhibiUgcCheck.json` (netblock) | 261 7ade7be6daae | 222 b62ed2d79db8 | no reader |

Eight winners change; `ball.fpkd` is the same bytes. The default match loads ball 1 (`DEFAULT_BALL_ID`), which sider does not override, so nothing GF loads by default changes on this install.

## Smokes

2 min, seed 7, one run per path, rc 0, both reached full time, run on the 41723be3 binaries under `simslot.sh`:
- switch-off: `[determinism] e663d1e917d6c4f4`, equal to main's `e663d1e917d6c4f4`. 33628 ticks, 24 stoppages.
- pes-loop: `[determinism] 980c54e27f93ced2`, equal to main's `980c54e27f93ced2`. 25942 ticks, 9 stoppages.

No separate rerun for determinism: each equals main's recorded baseline.

## GATE

`GATE fast: not run (interim).` The fast gate was queued on sim slots 3/2 for 8+ hours. Main told me to stop waiting: the background job is cancelled, no gate process of mine remains, and Main runs the fast tier on batch AR, which includes VfsOrder. The line is owed by that run.

## Build and git notes

- The first build after the merge did not finish in 1800 s. ninja's `VerifyGlobs` (from `CONFIGURE_DEPENDS`) is always dirty, so each run re-checks the globs. I re-configured with the gate environment (`PES_EXE`, `GF_DIFF_PYTHON`, `PES21_ROOT`, `GF_IMPURE_BANKS`). The red target then built in 780 s (1476 steps); the green targets in 232 s.
- `git fetch`, `status` and `push` need `-c fetch.recurseSubmodules=false`; an unscoped `git status` errors on the `extern/godot-cpp` symlink. All commits were path-limited. The untracked `godot/**.uid` files were left alone.
- No `#deps 0` objects after the `VfsOptions` layout change.
- PII: grep of the changed files for the machine path prefixes found nothing. `tools/check_pii.sh` was not run from the main checkout.

## Open items

1. CONFIRMED from `sider.ini`: the live game runs sider's live layer (`livecpk.enabled = 1`, `cpk.root = ".\livecpk\ball-root"`, Lua modules including `GameplayLoader.lua`). So the live game serves the 8 files above from sider, and default GF now does not. The ruling is stock; I flagged this and did not change it. Owner decision needed if GF should match the live game's sider files.
2. CONFIRMED by test: the default ball is a `download/` cpk copy (`VfsOrderInstall.DefaultMountServesTheBallFromTheCpks`). By raw TOC grep (INF), `ball021` is in `Data/dt15_x64.cpk` and `download/4cc_38_balls.cpk`. `topModeSelectDMM.bin` is also in `download/4cc_06_gameplay.cpk` and `4cc_10_interface.cpk`; its winner's layer is INF.
3. `PesLibrary::open` (gdext) takes the default, so Godot gets stock too. There is no Godot-side `sider` switch.
4. Gate not run (above).
5. Scratch: none kept. The build logs `build/vfsroots-*.log` sit in the worktree build dir and go with it.
