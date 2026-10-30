# pes-port-VfsOrder: GF's VFS resolves a file the way sider does (Lua modules > cpk.root > cpks); the 4cc_06 "winner" of constant_player.bin was never what the live PES loaded (30-10-26)

Branch `VfsOrder` (from origin/main d7d00588). Files: `src/pes/vfs.{hpp,cpp}`, `tests/pes_vfs_order_test.cpp`, `apps/gfsim_run.cpp`, `apps/pesfmt_dump.cpp` (both: `--gameplay-mod NAME`).

## 1. Answer

`constant::Trap` in the live PES equals the stock `trap.o` **not because PES prefers `dt18_all.cpk` over `download/4cc_06_gameplay.cpk`, but because sider's `GameplayLoader.lua` module answers before any cpk is consulted.** The live process had selected the gameplay mod `sixteen`; its `constant_player.bin` carries the stock `trap.o` (md5 of the trap.o record equals stock; 14 of the 22 mods in the install do, 4cc_06's does not). GF's VFS stopped at the cpk layers, so it never saw the module layer.

CONFIRMED evidence (live pid 1010053, read-only `sudo -n` preads of `/proc/<pid>/mem`, 30-10-26 ~09:00; scratch scripts `ctxscan.py`, `scan.py` in `<workdrive>/gf-VfsOrder-scratch/`):
- sider's memory holds the wide path `<PES21 install>\content\gameplay-loader\sixteen\common\match\constant\<f>.bin` for **all nine** constant files: constant_match, _player, _positionCK, _positionPK, _shootAging, _stadium, _team, _tutorial, _tutorialConsole (and the NT form `\??\<drive>...` = the CreateFile that opened them). No other mod's name appears in any path string.
- `sider.ini`: `livecpk.enabled = 1`, `lua.enabled = 1`, modules `netblock.lua` then `GameplayLoader.lua` (both register `livecpk_get_filepath`; the loader also `livecpk_make_key`).
- `content/gameplay-loader/config.ini`: `favorite_gameplay_mod = righetti4`, `is_random_gameplay_mod_enabled = true`, `load_no_mod = false`: `set_teams` re-rolls the mod at every match (uniform over the listed mods except "No Mod", `math.randomseed(os.clock())`), so the mod in force is a per-match input, not install data.
- trap.o per candidate (extracted with `trapo`): stock = sixteen = righetti4 = 13 more mods (md5 34e2b216), 4cc_06 = goblino = sixthree = ... (md5 1571eb1e, `+0x70` = 0x07). Live object matches the 34e2b216 class in 120/120 fields (TrapTick3 §3); sixteen is in that class.
- Raw `constant_player.bin` buffers are not resident in the process (49 unique 48-byte windows of every candidate, 0 hits), so the byte-level oracle for the **file** is the sider path string (above) plus the trap object; the object alone cannot separate sixteen from stock.

## 2. sider's rule (read from sider7 source `sider.cpp`, v7.4; the install runs 7.1.4, same functions in the log)

Per file request (`sider_get_size`, `read_file`, `lookup_file`): `have_cached(name)` (key cache, ttl 10 s, keyed by the requested name) → else `do_rewrite` (livecpk_rewrite modules) → `have_content` = every module in `sider.ini` order: `module_make_key` (default key = the name), `module_get_filepath`; a returned path counts only if the file exists; the first hit wins → else `have_live_file` = `cpk.root` folders in listed order, first existing wins → else the game's own cpk lookup. All of it needs `livecpk.enabled`; the module layer also needs `lua.enabled`. Source lines: `have_content` 3109, `_have_live_file` 1969, `module_get_filepath` 3041, dispatch 3180-3185.

GameplayLoader.lua (install's own module): `livecpk_make_key(ctx, filename)` returns `selectedMod\filename` when `selectedMod ~= "No Mod"` and `isGameplayFile(filename)`; `livecpk_get_filepath` returns `content\gameplay-loader\<key>`. `isGameplayFile` = the nine constant files plus `common\anime\`, matched with unanchored `string.match`. netblock.lua: key = the name, serves `content\netblock\<name>` if present (one file in this install, a flow json GF never reads).

## 3. What GF does now (`PesVfs::Mount(root, &err, VfsOptions{gameplayMod})`)

Mount order, later wins: Data cpks (number, `_4K_` after base, name) -> download cpks in DpFileList order (unchanged) -> `cpk.root` folders, reverse listed (unchanged) -> Lua modules, reverse `lua.module` order (new). Everything the modules need is read from the install's own files: content folder from the module source (`contentPath` / `content_root`), the mod list from `gameplay-mods.csv`, the boot selection from `config.ini`'s favorite (only if listed, else "No Mod"), the file filter from `isGameplayFile`'s `string.match` literals. A module that registers a livecpk event and is not netblock/GameplayLoader is reported in `MountReport()` as `UNMODELLED` (CustomExhibitionMatch registers only `livecpk_data_ready`: not an answer). `gameplayMod` = the loader's selection; unset = the loader's boot state (favorite righetti4 in this install). `vfs.GameplayMod()` reports it. `pesfmt_dump mounts --gameplay-mod sixteen` and `gfsim_run --gameplay-mod sixteen` reproduce the live state.

## 4. Which winners change (this install; `pesfmt_dump cat`, md5 prefix)

| file | before (4cc_06 / dt18) | boot righetti4 | sixteen (live) |
|---|---|---|---|
| constant_match | c6be09fa | f57e9bb9 | 29538000 |
| constant_player | 3332931d | 9dafbbac | 9b0dd722 |
| constant_team | a1371574 | 9b37d73c | b57c0df0 |
| constant_stadium | 55592fea | 8ead2b82 | 8ead2b82 |
| constant_tutorialConsole | 8e81ec48 | f085b335 | f085b335 |
| positionCK, positionPK, shootAging, tutorial | unchanged bytes | | |

No gameplay mod of this install ships `common/anime/`: no animation file changes winner. All other paths keep their winner.

**Behaviour moves**: every consumer of constant_player/match/team/stadium (movestep, contact, receive, defender decisions, trap, camera, ball, team concepts) now reads the selected mod's bytes. Pins that were captured against another mod need `--gameplay-mod` of the mod the capture was taken under (e.g. TrapTick3's 12 `LoaderDataTheTickReadsEqualsTheCapture` cases: select `sixteen`, the live mod, and the 0x0a vs 0x07 mismatch is gone because sixteen's trap.o is the stock one). **Decision for Main**: the default. I kept "the loader's own boot state" (favorite) because that is PES's deterministic state; PES itself re-rolls per match. If the gate wants the old bytes, pass `{.gameplayMod = "No Mod"}`, which gives exactly the previous winner.

## 5. Tests (`pes_vfs_order_test`, 11 tests) and mutations

Synthetic installs (7): module beats cpk.root; "No Mod" falls to first-listed root; only `isGameplayFile` files are served (anime prefix, other file hidden); module order from sider.ini (both orders); commented module and `lua.enabled = 0` answer nothing; explicit mod / "No Mod" / unlisted mod refused; boot favorite only if listed (CRLF csv, commented line). Install (4, `PES21_ROOT`): every listed mod's constant files resolve to that mod's disk bytes (all 22 mods); "No Mod" winner is one of the cpk variants; sixteen's trap.o record equals `Data/dt18_all.cpk`'s (the live oracle, a relation); a download cpk overrides Data for differing `common/demo/fixdemo` files.

Red first: on origin/main the 3 behavioural synthetic tests fail (commit 8d645e6b); two negatives pass trivially there.

Mutations (each applied, rebuilt, run, reverted; worktree file equals the commit afterwards):
- M2 modules mounted in listed order -> `ModulesAnswerInSiderIniOrder` fails.
- M3 filter `accept` ignored -> `LoaderServesOnlyWhatIsGameplayFileAccepts` fails.
- M4 `lua.enabled` ignored -> `DisabledModulesAnswerNothing` fails.
- M1 modules mounted before the roots -> `LoaderModBeatsLiveRoots`, `ExplicitGameplayModReplacesBootState` fail.
- M6 an unlisted favorite accepted -> `BootStateIsTheListedFavorite` fails.
Not mutated: forward/reverse `cpk.root` order (unchanged code, `NoModFallsThroughToFirstListedRoot` covers it on the synthetic set), the install tests.

Other binaries run on the branch (default = righetti4): `pes_movestep_test` 114/114, `pes_uniform_test` 36/36, `pes_camera_test` 17/17. Not run: the sim-linked pes_* tests and the gate (Main gates); `GATE fast`: not run (the machine load; first full build of this worktree took 39 min).

## 6. Native cpk order (Data < download, DpFileList order): code unchanged, live proof INCONCLUSIVE

The cpk layer is unchanged. `DownloadCpkOverridesDataCpk` pins "download beats Data" for the differing `common/demo/fixdemo` files that `dt12_g4.cpk` and the `4cc_*` cpks share (GF returns the download bytes).

Live check attempted (read-only scan of the rw maps of pid 1010053, 30-10-26; `conflicts`, `winmake`, `winscan.c`, `livewin.py`, `windows.tsv` in `<workdrive>/gf-VfsOrder-scratch/`): 1169 paths live in 2+ native cpks; for the 365 with distinct content, up to 2 unique 32-byte windows per variant were searched. 102 files have at least one variant resident. Of the 37 files that have both a Data and a download variant: 8 show only the download variant resident (e.g. `playerappearance.bin`, `unicolor.bin`, `uniformparameter.bin`, `ent_obj_flag_acl_02.fdc`, `cupstand_uefa_cl.fpkd`: download beats Data, as GF), 6 show only the Data variant (`ent_001_view_st007_mob.fdc`, `ent_001_view_st071_mob.fdc`, `ent_009_st000_south_mob.fdc`, three `passage_st002` ftex), 23 show both (ball fpks). Among download-only-vs-download variants, `glove.bin` shows `4cc_03_stockkit` (last mounted of its four variants) and `uniformparameter.bin` `4cc_68_midcup`/`4cc_08_bins` (last of its set), as later-wins predicts.

Verdict: **INF, not CONFIRMED.** Resident bytes are stale heap as well as live buffers (the bot cycles stadiums and balls; freed buffers are not cleared), so a Data-only hit can be a buffer from an earlier read through another path. A conclusive oracle needs the cpk name of each read: sider's `livecpk_data_ready` event passes `cpk_filename` (a 10-line logging module, `filedump.lua` is the shipped sample). Asked of Main: enable such a module at the next PES restart (sider.ini is Main's); then the 37 two-layer files can be settled by log, not by heap forensics.

## 7. What the loader answers is not stale heap

The sixteen result in §1 is a path string for all nine files in sider's own buffers, plus the trap object: that part is CONFIRMED.

## 8. Open items
1. The per-match random selection of the loader is an input, not modelled (`os.clock` seed). If the rendezvous captures record which mod was live, replay needs `--gameplay-mod` per capture.
2. `PesLibrary::open` (gdext) mounts with the default; no Godot-side selector yet.
3. sider's key cache (ttl 10 s, keyed by the requested name, not the key) can serve the previous mod's path for up to 10 s after a re-roll; not modelled.
4. The Lua `string.match` filter is case-insensitive here (Lua's is case-sensitive): ponytail comment in `vfs.cpp`.
5. Native order of two download cpks (DpFileList order) has no live proof yet.
