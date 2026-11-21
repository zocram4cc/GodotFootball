# EngineFixture: engine-memory fixture (T1) and capture-set registry (T2)

Branch `EngineFixture`, pushed: `7b7f7cdff5f52567da356aaae7b8344cafc209f8` (ls-remote confirmed). Base: `origin/OneWorld-series-wip` (2be76714), per the assignment. Plan: `docs/plans/pes-plan-integration-map.md` section 3.

## What landed

T1, engine-memory fixture:
- `tools/dev/translate/readset.py --windows OUT` (additive). Per call: the replayed read and write set, clustered per source into windows, one TSV per capture. Sources: `os_arena` (the VirtualAlloc model arena, `import_stubs.json`), `teb`, `stack` (within 1 MiB of entry rsp), `match` (the rest of the capture).
- `src/pes/translated/pes_state.hpp`: test seam `PesMem::read_miss`. A read that finds no region still returns zero; the seam is called only on that path. Null in production.
- `tests/pes_engine_fixture.hpp`: window-file reader, the standard-source check with a named failure, and `ReadMissLog`.
- `tests/pes_engine_fixture_test.cpp`: self tests, and the 72 kind-0 captures run through the fixture with the arena lent and withheld. Target `pes_engine_fixture_test` (in tests/CMakeLists.txt).
- `pes_trap_oracle_test` is not touched and not migrated.

T2, capture-set registry:
- `tools/dev/capsets/registry.py`: validates the registry; `GF_CAPSETS` names it (run/capsets.json, outside the repo; no machine path in the repo). Statuses: active, cited, protected. Active and protected are guarded.
- `tools/dev/capsets/check.py`: broken `mem.bin` oracle links (same set as `find ROOT -maxdepth 4 -xtype l -name mem.bin`), and registry captures missing on disk. Lists with `--list`; removes nothing. Exits 2 only when the registry is invalid. Wired into `tools/dev/gate.sh` as a fast step.
- `tools/dev/capsets/gen_oracle.py`: writes `manifest.json` per set with real paths, no symlinks. Refuses (exit 1, writes nothing) when a capture is missing.
- `tools/dev/rendezvous/capz.py`: `pack` refuses guarded captures. Unpack is unchanged.
- `tools/dev/rendezvous/auto_slim.py`: candidates and stale-full packing skip guarded captures. Exits when the registry is unreadable (fail closed).
- Seeded `run/capsets.json` (outside the repo, Main's run dir): 35 sets. Kind-0 (cited), traptick, anime_tick (cited); chainx, height, strike, kick chooser/header/root, judge, KickChooser/KickJoints/TeamTick/RestingBall/AnimeLayer/cut-anchor protected lists from brief-rules.

## Acceptance

Arena mutation (`pes_engine_fixture_test`, 72 kind-0 captures, one GF-side run each, `GF_ENGINE_ORACLE` + `GF_ENGINE_WINDOWS`):
- Arena lent: 72/72 captures read only mapped memory (all pass).
- Arena withheld: pid3727195 seq027 to seq037 (11 of 11) each read an arena address first, named by the test: seq027 `0x3000f50880`, seq028 `0x3000f50c00`, ..., seq037 `0x3000f52b80` (2 unmapped reads each). Log: `<workdrive>/gf-EngineFixture-scratch/logs/round11_arena_removed.log`.

Registry, check, refusal, tests: see Tests below. Broken oracle links: **207** (matches the plan's count). Registry captures missing on disk: **132 of 318** in 23 sets (the list is in `scratch/broken_links.txt`).

## Tests run

- `pes_engine_fixture_test` self tests: 3/3 pass (read-miss seam; named unmapped window; malformed windows file rejected).
- `pes_engine_fixture_test` kind-0 captures: 72/72 arena-lent pass; arena-withheld pass for the eleven round-11 captures' naming (above).
- `tools/dev/capsets/test_capsets.py`: 13/13 pass. The guard tests failed red first (TypeError on the old signatures), then passed.
- `tools/dev/rendezvous/test_auto_slim.py` 2/2, `test_capz.py` 5/5: pass.
- `tools/dev/translate/test_readset_windows.py` (harness Python): 8/8 pass. Registered in CMake only when `GF_DIFF_PYTHON` is set.
- Not run: the full gate (agents run targeted tests only, per 17-11).

## Findings (CONFIRMED unless marked)

1. **The plan's expectation for T1 does not hold as written.** The capture-side replay (readset, real VAs) reads no arena address for seq027: 0 of 1246 distinct reads and 0 of 945 writes. The arena reads come from the GF-side chain (the engine's `NewPlayer` chain lives in the arena, see OneChainCut round 11). So the read set that matters is the GF-side run; readset windows alone cannot show the arena failure. The fixture therefore detects misses in the GF-side run, through the seam.
2. **With the arena withheld, all 72 captures read unmapped memory**, not only the eleven. Only seq027-037 change output (per OneChainCut, and the eleven refusals). The other 61 read arena memory whose value does not change their output (the first miss is an arena address in each). This is a clarification for OneChainCut: "reads arena" is not the same as "output depends on the arena".
3. **Trap call inputs differ from Build.** The GF-side run is a minimal reconstruction (world bound, records and P from the manifest, request words, ball samples, the capture's match windows lent). It is not `Build` from `pes_trap_oracle_test.cpp`, which is OneChainCut's file and was not migrated. The duplicated code should go once OneChainCut lands a shared header. Output is not compared, only reads.
4. **Registry drift.** 132 of 318 captures in the seeded registry are gone (the brief's KEEP/PROTECTED lists include launch folders `pid615629`, `pid802552`, `pid2670420` and many single seqs). The check reports them; nothing was deleted.

## Decisions

- capz pack refuses guarded (active and protected) captures. Pack is lossless (zstd, checksum-tested), so it is not counted as slimming or deleting, but the guard follows the brief literally. auto_slim refuses guarded captures for both slim and pack.
- Gate: the capset check is a fast step; it fails the gate only on an invalid registry. Broken links are reported, as asked.
- `GF_CAPSETS` is required by capz pack, auto_slim, and check. Without it they exit (test: rc 2 for check).
- The `.capz.lock` in the round-11 folders is root-owned (created by the old root auto_slim). capz cannot lock it; I unpacked and repacked through `capz.unpack`/`capz.pack` directly, with no other packer running (auto_slim was stopped, per Main). Main can `sudo -n chown` the lock files if capz is to run unattended.

## Worktree notes

- `extern/godot-cpp` is a symlink; the commit tool refuses symlinks there. I swapped it for an empty dir for the single commit, then restored the symlink. Pushed commit contents are unaffected.
- Build: `build/` configured in the worktree. `pes_engine_fixture_test` built with `flock /tmp/gf-build.lock nice -n 19 cmake --build build --target pes_engine_fixture_test -j8` (1601 s, mostly the generated translated layer, ccache mostly cold).

## Open items

1. OneChainCut: share the minimal run's builder with `pes_trap_oracle_test` (one header) when that branch lands. Until then the two copies must agree.
2. Main: confirm the registry statuses for the protected sets whose launch folders are gone; decide whether the 132 missing entries should be dropped from `run/capsets.json`.
3. The GF-side run's reads are not yet tied to outputs. A stronger check would compare each arena read's value to PES's and report which reads change the output.

## Round 2 (18-11, `aebe3181`)
# EngineFixture round 2 (Main's decisions)

Commit aebe31810c (pushed, ls-remote verified). Tests: capsets_test 24/24 (new refusal, default path, mark-gone, gone manifest cases), test_auto_slim and test_capz pass.

1. capz pack refuses only status active (an agent replaying it); protected captures pack. auto_slim: slim refuses active and protected; pack refuses active only. No deletion path exists in these tools.
2. GF_CAPSETS unset: the registry resolves to the standard run file beside the checkout (gf-Rendezvous-scratch/run/capsets.json). Test: the default and the override.
3. Status gone. `registry.py mark-gone --write` marked 23 entries (132 captures) gone; none needed a split. Their docs are kept. check.py reports live-set missing (0 of 186) apart from gone (132 in 23 sets). Broken oracle links: 207 (unchanged).
Registry backup before the write: gf-EngineFixture-scratch/capsets.before-gone.json.
