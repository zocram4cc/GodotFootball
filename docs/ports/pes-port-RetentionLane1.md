# RetentionLane1: footprint slim, archived full copies, named refusals, keep set, expiry dry run

Branch RetentionLane, tip `7bf30de9a` (pushed; `git ls-remote` confirms). Base origin/main `9502a495d`.
Commits: `0819f630a` red tests; `64e272dcb` tools (root causes A, B, D, E; keep set; expiry); `cc40ae145` test for root cause C;
`a960ed749` root cause C fix; `7bf30de9a` docstring.

## 1. What changed (M6: root cause, red test, mutation check per change)

- **A. Clean-return bug (slim.py `replay`).** `frame_trace.recorded_main` returns 1 for a run that did not return (unmodelled
  import, fault, step cap). `replay()` set `clean = True` after any normal return, so such a run was slimmed as verified.
  Fix: `clean = rc == 0`. Test `test_a_run_that_does_not_return_keeps_the_capture_full`. Mutation M2: red.
- **B. Stand-in read set (footprint).** The read set came from a run with GF stand-ins (`stubs.json` models) in place of PES
  callees, so pages PES's own allocator and stack code read were never read and were dropped. Fix: `frame_trace.py recorded
  --no-stubs` (new flag); PES's image bytes run from hook entry to return. Test `test_footprint_keeps_the_page_a_standin_would_skip`.
  Mutation M1: red.
- **C. Export-entry pages (found by the acceptance run).** `identity_reads` kept each module's header and export tables but not
  the page each export entry sits on. `LazyMap._fetch` maps an entry's import model only after `SnapMem.page_ok(page)`, a check
  that reads nothing, so a full capture never logged that page. The slim dropped it and refused the first call through it:
  `case excluded: page not captured at 0x6f59afdb6000` (rc 1). Fix: `identity_reads` reads one byte of each export entry's page.
  Test `test_an_export_entry_page_is_kept_although_no_replay_reads_it`. Mutation M11: red.
- **D. Full original deleted, no pointer.** `trim` deleted the full `mem.bin` after verifying the slim. Now: `index.json` `slim`
  holds every dropped range as `[VA, bytes]` and `full`, the archive-relative path `pidN/seqM/mem.bin.zst`. The full original is
  packed (zstd -3), copied to `$GF_CAPTURE_ARCHIVE` (env only; no machine path in committed files), verified byte for byte with
  `cmp`, then the local packed name becomes a link and the full `mem.bin` is removed. Any failure restores the full `mem.bin` and
  `index.json`. `slim.py --restore DIR` unpacks through the link and drops the `slim` record. `SnapMem.dropped_reason` names the
  archived copy and the restore command. Tests `test_a_trimmed_capture_records_its_drops_and_names_its_archived_copy`,
  `test_the_full_original_is_archived_and_restores_byte_for_byte`. Mutations M4, M9: red.
- **E. capz lock (capz.py `locked`).** It opened `.capz.lock` for writing. A lock file left by a root packer (root:root 644,
  present in pid506456's capture dirs) made every user pack or unpack die with PermissionError. Fix: read-only open with flock.
  Test `test_capz_lock.py`. Mutation M8: red.
- **Keep set (auto_slim.py).** Never slims the first `KEEP_FIRST_IN_PLAY` = 2 in-play captures of each hook per launch. In play
  is the watcher's predicate `d(q(q(0x1436F3DC8)+0x270)+0x116c)==1` (MI+0x116C == 1), evaluated on the capture's memory and cached
  in `index.json` as `in_play`. Also never slims a capture named by a keep file line `pidN/seqM` (or a hook name, as before).
  Tests in `test_auto_slim.py` (`FirstInPlayStaysFull`). Mutations M6, M10: red.
- **Expiry dry run (expire.py).** Lists captures older than `--days` (14) that no `--record` results file names (`pidN/seqM`, or a
  launch `pidN`, which protects every capture of that launch) and no `--cited` lane file names. Prints each capture and the total
  bytes. It has no delete option. Test `test_expire.py`. Mutation M7: red. Created `run/cited.txt` (empty, header comment) for lanes.

Known limit: `LazyMap` maps 256 KiB blocks on first touch, so the footprint is block-granular. It is a superset (safe, coarser
than page-exact). Slim copies come to about 31 MB each.

## 2. Acceptance: live2/pid506456 (relaunch 7)

- In play: 36 of 36 captures (MI+0x116C == 1). Keep set: seq206 and seq207 (ringw1_140A76440), seq213 and seq15454 (aimw_140514940),
  the first two in play per hook. These stay packed and full.
- Slimmed: 22 ringw1 and 10 aimw.
- Pass 1: all 22 ringw1 slims failed the slim replay (root cause C). The 10 aimw slims were made before C was fixed, so each was
  restored from its archive copy and re-slimmed with the current tool (10 of 10).
- CONFIRMED, replay equality: for each of the 32 slim copies, the proofs' own trace (`frame_trace recorded`, stand-ins on, as
  snapdiff records it) ran on the full original and on the slim copy. Both return rc 0, and the stdout JSON and trace tree are
  identical: 32 of 32.
- Records: PassLane's `tools/dev/translate/results_trapaction.json` (worktree gf-PassLane, 5403c2ee9) cites "the 24 real ringw1
  calls of live2/pid506456" for 0x140A76440 and its caller. 22 of those 24 are slimmed and replay bit-identically. The other two
  (seq206, seq207) stay full (packed; unpacked only to read their in-play flag, then packed again). The record itself was not edited.
- NOT RE-RUN: the prove_bounded/snapdiff verdict (PES bytes against the translated closure). It needs PassLane's 0x140A76440 spec
  (not on main) and a translated build (none in this worktree). Replay-level evidence only.
- CONFIRMED, archive integrity: 36 of 36 packed copies pass `zstd -t` (32 through their archive links, 4 local).
- CONFIRMED, hole refusal naming the archive (seq11641, dropped page 0x12000): `SnapMem.read` returns None and the refusal is
  `case excluded: page 0x12000 was dropped by footprint slim; the full copy is $GF_CAPTURE_ARCHIVE/pid506456/seq11641/mem.bin.zst;
  restore with: python3 tools/dev/rendezvous/slim.py --restore <live2>/pid506456/seq11641`.
- The `.unpacked` markers left by the unpack steps were removed from the 32 slim captures (they are not data).

Disk, launch pid506456: local 34 GB (36 packed captures) -> 4.7 GB (4 kept packed captures ~3.9 GB; 32 slim copies ~1.0 GB total).
Archive: 30 GB in 32 packed full copies under `$GF_CAPTURE_ARCHIVE/pid506456/`. Full originals are 4.0 GB each unpacked (transient
only). `df <workdrive>`: 526 GiB free before, 575 GiB after (other agents also moved data). Archive drive: 459 GiB before, 393 GiB
after (other archive moves are in transit; not attributable to this launch).

## 3. Expiry dry run

- With the records of main and every gf-* worktree (2,446 results files, launch-level citations): 0 eligible.
- Upper bound with no records (older than 14 days, not in cited.txt): 41 captures, 40.98 GB. Lists in the scratch directory, not
  committed. Nothing was deleted.

## 4. Checks

- Rendezvous tests (unicorn venv): test_slim_footprint 5/5, test_auto_slim 4/4, test_expire 3/3, test_capz_lock 1/1, test_capz 5/5,
  test_pes_capd 6/6, test_rv_sweep 6/6, test_slim_identity (4 checks).
- Translate tests: test_recorded_call 12/12, test_image_reads 9/9, test_image_identity OK.
- Red before green: the tests in `0819f630a` failed on the old tree (trim arity, missing expire module, lock PermissionError, in-play arity).
- Mutations, each run in a scratch copy and reverted (all red): M1 `--no-stubs` removed; M2 clean = True; M4 archive name removed from
  the refusal; M6 KEEP_FIRST_IN_PLAY = 0; M7 launch-level citation dropped; M8 lock opened RDWR; M9 full original not removed;
  M10 keep-file captures ignored; M11 export-entry read removed.
- `tools/check_pii.sh` rc 0 on 7bf30de9a (run with the godot-cpp symlink swapped for an empty directory; restored afterwards).
- Gate: not run (Main gates). No C++ build in this lane.

## 5. Open items

1. Restart auto_slim with this code and `GF_CAPTURE_ARCHIVE` set (`--keep-file` for agent lists). I did not touch the running
   services; Main owns them.
2. Deletion needs owner approval: the expiry list (41 captures, 40.98 GB upper bound) is unapproved. Records cite launches, not
   captures, so a capture-level citation rule would free more.
3. Re-run PassLane's 0x140A76440 proof (prove_bounded/snapdiff) on the 22 slim copies once the spec and a build exist.
4. Refusal path tested in production only by the named refusal above; `--restore` is unit-tested, not run on pid506456.
5. Footprint granularity: 256 KiB blocks. A per-access footprint would keep fewer pages (not done).

## 6. Record verdict re-run on the slim captures (0x140A76440; closes the "NOT RE-RUN" item in section 2 and open item 3)

Record: PassLane's 0x140A76440 proof (`tools/dev/translate/results_trapaction.json` row 102, `closure_passlane3_ring.json`), re-run on PassLane tip 5403c2ee9 in a detached scratch worktree (`gf-RetentionLane-scratch/passlane`).

Command: the record's own command (`docs/ports/pes-port-PassLane3.md`), minus `--merge` (nothing is written into the repo) and without `--native` (the record did not use it):
`PES_EXE=pes_full.bin RV_HOOKS=pid506456/hooks.cfg GF_DIFF_PYTHON=<venv> prove_bounded.py 0x140A76440 <work> --tag passlane3_ring --real <24 dirs>`

Inputs: the 24 `ringw1_140A76440` captures of pid506456, with the launch's own `hooks.cfg`. 22 are footprint slim copies, used as they are: their `mem.bin` is the slim sparse file, never unpacked (`capz unpack` would restore the archived full copy). seq206 and seq207 are kept full: unpacked for the run, packed again afterwards.

| | record (row 102) | re-run on the slim captures |
|---|---|---|
| calls / replayed | 24 / 24 | 24 / 24 |
| mismatches | 0 | 0 |
| excluded, aborted, nonreturn | 0, 0, 0 | 0, 0, 0 |
| own block | 16 of 60 (0.2667) | 16 of 60 (0.2667) |
| closure | 19 members | built set minus the root = the same 19 VAs |

Verdict (`work/prove.log`): `0x140A76440: proven=True calls=24 replayed=24 excluded=0 mismatches=0 own_block={'reached': 16, 'total': 60, 'fraction': 0.2667}`

Checks: 24 traced calls over 24 captures; the harness and translated rows agree on rax, xmm0 and written bytes for all 24 (`corpus.jsonl` against `translated.jsonl`). Every translated run returned rc 0. Wall time 43 s. Work dir `acceptance/verify/` is 9.6 MB.

Hole refusals: none (`reasons` empty, excluded 0). No slim defect surfaced on this path, so no fix was made.

Scope: the only results record that cites pid506456 is this one (grep over every worktree's results JSON). The caller record 0x142106110 cites live2/pid2108145, not this launch, so it is unaffected. Records are not changed: `results_trapaction.json` and `closure_passlane3_ring.json` are untouched.

Open, not changed in this step (decision for Main):
- `capz.status` reports a slim capture as `packed`, because its `mem.bin.zst` is an archive link. `capz.py unpack` on a slim capture replaces the slim `mem.bin` with the archived copy's bytes, removes the link and keeps the slim record. Confirmed on a synthetic slim capture in scratch (`status` printed `packed`; `unpack` returned `unpacked`; `mem.bin` then equalled the archived copy). No data is lost, and this verdict did not unpack any slim capture. Proposed M6 fix: `status` returns `slim` for an archive link; `unpack` skips archive links; `slim.restore` unpacks through the link explicitly. Red test in test_capz first.
- The PassLane scratch worktree is detached and can be removed with the rest of the scratch.

## 7. capz: archived slim captures read slim, and unpack refuses them (M6; closes the open item in section 6)

Root cause: a footprint slim capture's `mem.bin.zst` is a link to its archived full copy. `capz.status` tested only that the name exists, so these captures read `packed`. `capz.unpack` then took the link as a packed copy: it replaced the slim `mem.bin` with the archived bytes, removed the link and kept the slim record, so a full restore happened as a side effect. Introduced with the footprint slim (64e272dcb). No data was lost.

Change: red commit `459d71a26` (tests), fix commit `8c26b0ea9` (tip, pushed; `git ls-remote` confirms).
- `archived(d)`: `mem.bin.zst` is a link. `status`: an archived capture is `slim`.
- `unpack`: an archived capture raises `Refused` with `slim capture: its full copy is archived; restore it only by an explicit act: python3 tools/dev/rendezvous/slim.py --restore <dir>`. The CLI prints the refusal on stderr, exits 1 and still processes the other captures. Nothing is changed.
- `restore` (capz): the explicit restore, archived captures only. `slim.restore` uses it, and `slim.py --restore` exits 1 on a refusal.
- Unchanged: packed and full captures. An old sparse slim capture (no archived copy) still reads `slim`, and `unpack` skips it, since there is nothing to restore. This is a judgment call: refusing it would point at a restore that cannot happen.
- Docs and callers: rendezvous README, tools README (synth.py), `test_goal_cast.py` docstring (env-gated: a footprint slim capture needs `slim.py --restore` first, since its reads go beyond the recorded call), CTest comment.

Tests (`test_capz.py`, class `SlimCapture`, 7 tests; ctest `capz_test` runs all 12):
- Red at `459d71a26`: six failures for the expected reasons: status `'packed' != 'slim'`; unpack returned `'unpacked'` instead of refusing; a refused unpack changed the slim copy and the archive; the CLI exited 0; `capz.restore` did not exist (two tests). The packed-local guard passes.
- Green at `8c26b0ea9`: test_capz 12/12; test_capz_lock 1/1; test_auto_slim 4/4; test_expire 3/3; test_slim_footprint 5/5; test_rv_sweep 6/6; test_pes_capd 6/6; test_slim_identity checks ok.
- Mutations, each on a fresh scratch copy (the repo untouched, scratch removed); each turned its tests red:
  - M1 status ignores the link: test_status.
  - M2 unpack does not refuse: the refusal, state and CLI tests.
  - M3 explicit restore always refuses: test_capz explicit restore (error) and test_slim_footprint restore (error).
  - M4 CLI exits 0 on a refusal: the CLI test.
  - M5 refusal drops the restore pointer: the refusal and CLI tests.
  - M6 refused unpack still changes the capture: the state test only.
  - M7 slim.restore goes through unpack: test_slim_footprint restore (error).

Real launch (pid506456), read-only: `capz.py status` gives 32 slim and 4 packed; the 32 are the archived captures that the old check read as packed. `capz.py unpack` on archived seq11641 is refused: exit 1, the message names `slim.py --restore`, and its link, slim `mem.bin` (about 30 MB allocated) and index are unchanged, with no `.unpacked` marker.

PII: `tools/check_pii.sh` rc 0 (the godot-cpp symlink was swapped for an empty directory for the check, then restored).

Open, for Main:
- Restart auto_slim with `GF_CAPTURE_ARCHIVE` once RetentionLane reaches main (your action; not touched here).
- Old sparse slim captures still read `slim` and unpack skips them. Say if they should get the restore message too.
