# PressLane RecordLane1: 0x14063DE20/DD20 root cause, core re-check, layer state

Branch PressLane, pushed head `9fb64855c4cb1cafc9e80f7854f4cad9483a4ae3` (`git ls-remote origin PressLane` confirms it). Base tip 0127b131 (driver fix).

## Verdict

- 0x14063DE20 and 0x14063DD20 are **bit-exact now** (64/64 each). Their failure was a harness bug, not a translation bug and not a stack-base difference.
- 0x140928FC0 (the press tick) is **still held out**. It is no longer held by 0x14210F110 (that callee is committed), but its 653-member closure now reads 59 live sites of read-only image tables (6 tables, 11 functions). The lint refuses to commit those reads (GF maps no image there). The gen now holds the tick out with that reason. It needs `ro_tables.json` declarations (or baking) for the six tables below. I did not do that work inside this budget.
- 0x14202BA30 is **released** (its closure callee 0x14202C250 is now bit-exact, 75/75 cases).
- 0x140555110 and 0x1405549C0 (pre-existing hold-outs) are **released** by the same harness fix.
- 0x14063CD90 has no record change: still "harness cannot run" (1 of 64 valid). It is a closure member; its translation is exercised bit-exact inside DE20's 64 cases.

## 1. Root cause of 0x14063DE20/DD20 (CONFIRMED)

Evidence chain:
1. Reference (PES under unicorn, `run.py`) returns eax = 0x21400001 in all 64 cases, a constant. The C++ returned eax = 0x00000001.
2. The DE20 callee chain ends `call 0x14063DC10; ... call 0x14063CD90`, then DC10 does `cmp dword [rsp+0x64],0; sete al`. So eax bits 8..31 are whatever CD90 left in rax. CD90 leaves rax = the value loaded at 0x14063CE40 `mov rax,[rbp-0x68]`, which holds `*(*(0x1436F3DC8)+0x270)` (the MI pointer).
3. Reference trace (scratch `dh/run.py` with a per-VA hook): `TRACE 0x14063ce40 rax=0x21400000`, so **PES's MI pointer at IMG+0x270 is 0x21400000**. The C++ run got 0.
4. Spec `specs/core/14063DE20.json` has both an input `IMG_1436F3DC8_270` (an integer stand-in, 227) and `regions_init` `IMG.1436F3DC8+0x270 -> IMG.1436F3DC8.E1EE` (a pointer). The same clash occurs on IMG+0x0 (and IMG+0x40 for 140555110).
5. The two harnesses disagree on which write wins. `tools/dev/diffharness/run.py` (the reference) writes inputs first (`set_inputs`, line 841), then `patch_addrs` and `regions_init` (lines 849-854), so the linkage pointer wins. `tools/dev/translate/diffmain.py` (the C++ diff main) wrote `regions_init`, then `patch_addrs`, then inputs, so the integer 227 won.

Root cause: **the C++ diff main's write order differs from run.py**. The C++ side ran with MI = 227 and the reference with MI = pointer, so rax differed only in bits 8..31.

Fix (translator harness, `tools/dev/translate/diffmain.py`): the inputs block now runs before the linkage blocks, matching run.py (`inputs -> patch_addrs -> regions_init`). Patch and linkage writes are unchanged in relative order; they touch disjoint addresses.

Red/green: before the fix, DE20/DD20/140555110 gave `ret_eax want 21400001 got 1` (64/64 mismatch, `results_core` run3). After the fix: `DE20 64/64 bit-exact`, `DD20 64/64`, `140555110 64/64` (scratch `corerun/run4.log`).

Mutation check: reverting the diffmain move is the "before" state above (the same spec, the same VA, a mismatch in every case). Not rerun as a separate ctest.

Scope of the harness change (`diffmain.py` is in `select_recorded` GLOBAL):
- 1,220 specs in `specs/*/` have an input naming a `regions_init` slot. 624 of them held a diffed record.
- I re-ran the clash-affected diffed records in every results file (core 527, pilot 61, cutscene 18, presentation 10, impure 4).
- Result: 507 core rows stay bit-exact, with no status change. 20 rows in the clash set are `proof: snapdiff` (14 of them in core; the rest in the same set), which the driver never re-checks (`driver --check` skips them, and snapdiff replays snapshot memory, not this harness). Their rows are kept as recorded. The rest of the re-run rows changed no status.
- Other sets: 0 status changes except 0x14202C250 (cutscene), which went from "translator bug" to bit-exact 75/75. Its old corpus file was stale: the mismatch values ("want 8000000000000000") do not reproduce from the current `run.py` with the spec seed (`dh` reproduction: cases 73/74 give the same values the C++ gives).

## 2. CD90 (0x14063CD90): not a divergence

- Its own record is "harness cannot run": `unmodelled read at 0x1b`. The spec sets only rcx. DC10 calls CD90 with rdx, r8d and r9d as well (`call 0x14063CD90` with param_2, param_4 and bVar4), and the spec never sets them. That is a spec gap.
- Evidence that the translation is right: CD90 runs inside DE20's and DD20's 64 real cases, and the whole closure is bit-exact against the reference (`ret_eax`, the only output).
- Not fixed: a record needs a spec for rdx/r8/r9 (specgen). Open item, not on the tick's path.

## 3. Record procedure (what was actually done, and why not the brief's)

- `driver.py --record` writes only on a full run (no `--only`), and `--set` does not filter (it only attributes). So a subset run cannot write `--record`. I used `--only VA ... --spec-dir tools/dev/diffharness/specs/<set>`, which writes the build copy `OUT/results.json` containing exactly those rows.
- Merge: `json_union_merge.py BASE OURS THEIRS` with BASE = THEIRS = the full merged3 (or the set's file), and OURS = the same file with the re-checked rows replaced (`merge/splice.py`, which keeps `proof: snapdiff` rows). I verified that BASE = merged3 minus the re-checked rows and OURS = the small record gives a conflict on every re-checked row (`both sides changed it differently`, exit 1). That is because both sides add the VA with different content. So the brief's suggested shape does not merge.
- Commands (in the worktree, with `GF_DIFF_PYTHON=<workdrive>/gf-DiffHarnessRE-scratch/venv/bin/python GF_DIFF_CACHE=$HOME/.cache/gf-diff-cache`):
  - `flock /tmp/gf-build.lock nice -n 19 $GF_DIFF_PYTHON tools/dev/translate/driver.py --image <home>/ghidra-projects/scratch/Unpacker/pes_full.bin --out-dir <scratch>/out --spec-dir tools/dev/diffharness/specs/core --set tools/dev/diffharness/specgen/sets/core.txt --only <VA ...> --jobs 3`
  - `python3 merge/splice.py <orig.json> <out>/results.json <ours.json>` then `python3 tools/dev/translate/json_union_merge.py <orig.json> <ours.json> <orig.json>`
  - `flock /tmp/gf-build.lock nice -n 19 $GF_DIFF_PYTHON tools/dev/translate/gen_translated.py --image <pes_full.bin>` (write), then `... --check`.
- Gen runs are about 30 minutes each. Run them detached (`setsid nohup`): a foreground call hit the 1,800 s deadline and was killed.

## 4. Layer state

- `results_core.json`: 3,183 rows, 2,540 diffed (was 2,537 in merged3; +DE20, +DD20, +140555110).
- `gen_translated.py` (`tools/dev/translate/gen_translated.py`): a new hold-out rule, mirroring the mistranslated-callee rule. A recorded root whose closure reads a read-only image table live is held out with a printed reason, instead of aborting the whole run in lint. Without it the gen aborts: `59 live read(s) of a read-only image address`.
- Gen output (`corerun/gen5.log`): wrote 10,980 functions (4,155 recorded bit-exact, 6,825 closure). Held out: `0x140928FC0` (reason below). No other held-out lines.
- `gen_translated.py --check` on the committed tip 9fb64855: exit 0, "10980 functions (4155 recorded bit-exact, 6825 closure) regenerate byte-identical". Held out: 0x140928FC0 only.

## 5. Held out (all of them) and why

| VA | Reason | Evidence |
|---|---|---|
| 0x140928FC0 (press tick, trapaction proof) | Closure reads read-only image tables live: 59 sites, 53 image addresses, 11 functions. Lint refuses to commit a live read of .rdata. | `gen5.log` |

Live-read tables (cluster, sites, reading functions; lint, not yet declared in `ro_tables.json`):
- 0x1425DD660..0x1425DD950: 0x1406B7560, 0x1406BA900, 0x1406C0170.
- 0x1425E3990..0x1425E46D8: 0x1406F0660, 0x1406F4E80, 0x1406F6450 (25 sites).
- 0x1425E91E0..0x1425E91E8: 0x140781B30.
- 0x1425EE990..0x1425EEC80: 0x140809070 (13 addresses).
- 0x1425EFBE0..0x1425EFC18: 0x140842BE0, 0x140844CB0.
- 0x14263EC57..0x14263EDB0: 0x140A06DD0 (9 sites).

Next step to land the tick: for each cluster, decode the index guard in its reader and declare the extent in `tools/dev/translate/ro_tables.json` with a CONFIRMED VA citation (the existing entries are the model). Then regenerate, `--check`, and re-diff the tick's proof only if its closure changed.

Not held out, but read this way: 0x14202BA30 (released), 0x14210F110, 0x142119390, 0x14211D710 (released; they were held by DE20/DD20).

## 6. Open items

1. The six RO table clusters above (blocks the tick).
2. CD90 record spec (rdx/r8/r9 unset): "harness cannot run". Not on the tick's path.
3. `impure` records were re-run without `GF_IMPURE_BANKS` and stayed diffed (4/4). If a bank-only record exists, it was not exercised with its bank.
4. The 20 snapdiff rows of the clash set (proof from real captures; 14 changed status under the driver, so they stay as recorded) were not re-proven under the new harness, because snapdiff replays snapshot memory and does not use the harness the clash changed. Their verdicts stand as recorded.

## 7. Commands and scratch

- Scratch: `<workdrive>/gf-PressLane-scratch/` (corerun out3/out4/out5/out6_*, merge/, dh/ traced harness copy, dbg14063/ instrumented C++ copy).
- Gate: not run (Main gates). Gen check and `tools/check_pii.sh`: see the final message.
