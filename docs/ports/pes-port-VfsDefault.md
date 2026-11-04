# pes-port-VfsDefault: the default VFS mount is "No Mod" (stock PES); the loader's boot favourite is no longer read

Branch `VfsOrder` (worktree `<workdrive>/gf-VfsOrder`). Owner ruling 30-10-26: GF resolves files as stock PES, so no GameplayLoader mod applies unless one is named. Every SHA below is confirmed by `git ls-remote origin VfsOrder`, which reads `949be2b2` at the end.

| step | SHA | content |
|---|---|---|
| merge | 196baf7b | `origin/main` (682b52b7) merged into VfsOrder, no conflicts |
| red | c068d950 | `VfsOrder.DefaultMountIgnoresTheBootFavorite`, fails on 196baf7b |
| green | 50d9c54b | default is "No Mod"; favourite parse deleted; tests opted in explicitly; install guard `VfsOrderInstall.DefaultMountIsNoMod`; port doc §9 |
| fixture | 949be2b2 | synthetic `gameplay-mods.csv` lists mods only, as the real file does |

## Behaviour

- `PesVfs::Mount(root, &err)` with no `gameplayMod` selects "No Mod". GameplayLoader answers nothing and the game's own cpks decide: download/ over Data/, the stock constants.
- `favorite_gameplay_mod` in `content/gameplay-loader/config.ini` is no longer read. Nothing else consumed it.
- A mod is applied only when named: `{.gameplayMod = NAME}` or `--gameplay-mod NAME` (gfsim_run, pesfmt_dump). The sider order (Lua modules, then cpk.root, then cpks) is unchanged for that case. An unlisted name is still refused.
- Unchanged: Data/ and download/ order, sider's cpk.root folders, netblock.lua.
- `pesfmt_dump` usage now lists `[--gameplay-mod NAME]`. Flag comments in gfsim_run and pesfmt_dump say the unset default is "No Mod".

## Tests

- **Red** (c068d950): `DefaultMountIgnoresTheBootFavorite` fails. With `favorite_gameplay_mod = modA` listed, the default gave `GameplayMod()` = "modA" (want "No Mod") and `constant_player.bin` = "MODA" (want "ROOT_A"). The other 11 tests passed.
- **Green** (50d9c54b, fixture 949be2b2): `pes_vfs_order_test` 13/13, `PES21_ROOT` set, no skips.
- **Changed tests.** Four synthetic tests now name `modA` explicitly: `LoaderModBeatsLiveRoots`, `LoaderServesOnlyWhatIsGameplayFileAccepts`, `ModulesAnswerInSiderIniOrder`, `DisabledModulesAnswerNothing`. Three would fail under the new default. The fourth would pass without ever exercising the disabled module. `BootStateIsTheListedFavorite` became `ExplicitModIsReadFromTheLoadersList` (CRLF list, commented-out mod refused). `ExplicitGameplayModReplacesBootState` was renamed `ExplicitGameplayModSelectsThatMod`.
- **Install guard**: `VfsOrderInstall.DefaultMountIsNoMod` checks that the default equals an explicit "No Mod" mount on all nine constant files.
  - On this install, `config.ini` reads `favorite_gameplay_mod = No Mod`. The `righetti4` in the port doc's §2 is stale. So this test is green before and after here. It goes red on an install whose favourite is a listed mod. The red test is the synthetic one.
- **Fixture bug found by the mutation** (fixed in 949be2b2): the synthetic CSV listed "No Mod" as a line; the real file does not, since the loader adds it. With that line, `mods[1]` was "No Mod", which hid a first-listed-mod default from the synthetic tests.
- **Mutation M1**, default = first listed mod (`value_or(mods.size() > 1 ? mods[1] : "No Mod")`). Applied, rebuilt, run, reverted each time; `vfs.cpp` equals 50d9c54b afterwards.
  - Run 1, with the old fixture: only `VfsOrderInstall.DefaultMountIsNoMod` failed. That was the fixture gap.
  - Run 2, after 949be2b2: `NoModFallsThroughToFirstListedRoot`, `DefaultMountIgnoresTheBootFavorite` and `DefaultMountIsNoMod` failed.
  - Reverted: 13/13 again.
- **Other binaries** on the green tree, `PES21_ROOT` set, slot 2: `pes_movestep_test` 114/114, `pes_uniform_test` 36/36, `pes_camera_test` 17/17.

## Smoke (2 min, seed 7, `--pes21-root <workdrive>/PES21`, slot 2 switch-off, slot 3 pes-loop, both rc 0, both reached full time)

- switch-off: `[determinism] e663d1e917d6c4f4`, main `e663d1e917d6c4f4`. **Not moved.** 33628 ticks, 24 stoppages.
- pes-loop: `[determinism] 980c54e27f93ced2`, main `980c54e27f93ced2`. **Not moved.** 25942 ticks, 9 stoppages.
- Context: this install's favourite is already "No Mod", so the old and new defaults coincide here and identical hashes are expected. They would move on an install whose config names a listed mod. Determinism was not re-run as a second pass. Each path matches main's recorded hash.

## Gate

GATE fast: GREEN. `GATE_TIER=fast GATE_SLOTS="3 2" tools/dev/gate.sh` on 949be2b2 (reconfigured with PES_EXE, GF_DIFF_PYTHON, GF_IMPURE_BANKS, PES21_ROOT exported). ctest 100% passed, 2163 tests (6252 s, -j 2, SimMatch and gfsim_run_test excluded by the fast tier). Gate smokes, 2 min seed 7: switch-off e663d1e917d6c4f4 and pes-loop 980c54e27f93ced2, both equal to main and both reached full time. Recorded re-check: 0 of the recorded functions selected, no unrecorded status regression, LEDGER.md current.

## Open items

1. sider's `cpk.root = ".\livecpk\ball-root"` still mounts by default, as sider's own live layer. If "stock" should also drop sider's roots, that is a separate owner decision.
2. `PesLibrary::open` (gdext) mounts with the default, so Godot now gets "No Mod". There is still no Godot-side mod selector.
3. PES re-rolls the loader mod per match when `is_random_gameplay_mod_enabled` is set. GF takes the mod as an input and does not model the roll. Captures taken under a mod need `--gameplay-mod` for that mod (port doc §8).
4. `docs/ports/pes-port-VfsOrder.md` §2 to §5 still describe the old default and 11 tests. §9 supersedes them; they were not rewritten.
5. `git diff` shows `extern/godot-cpp` as modified only because the worktree symlink replaces the submodule directory. Nothing was committed for it, and the merge did not change the pin (507ed9d8 on both sides). Stage by path.
6. The build was reconfigured with `PES_EXE` and `GF_DIFF_PYTHON` exported (`cmake -S . -B build`) before the gate.
