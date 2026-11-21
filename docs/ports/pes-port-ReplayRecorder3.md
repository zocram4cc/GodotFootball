# pes-port-ReplayRecorder3: paired refcount rule landed; 1408BE3C0 proven on six real captures but NOT merged (its closure reads undeclared image tables); 1408BED00 still FAILS the guard (a flag store into the paired object, outside the ruling); the reset caller and the goal-replay frame caller decoded; C++ not rebuilt

Branch `Presentation`, pushed `77d69487..f2084bda`. `git ls-remote origin Presentation` = `f2084bda1eb0a7e53ac917932c139277d3ab13b0` (confirmed). One commit, `f2084bda`: boundary_guard.py (paired rule), test_boundary_guard.py (20 tests), ro_tables.json (0x142B67EF0 corrected, eight 0x141EBBDB0 windows), src/pes/translated/closure/pes_fn_141EB6840.cpp (regenerated, its window constant only). Build dir removed; capture copies removed; scratch 47 MB (logs, rowcount.py, the held 1408BED00 record diff from round 1).

Labels: CONFIRMED = a unittest run, a prove_bounded run, a headless read of the flat image (`pes_full.bin`, file offset = VA - 0x140000000), a trace replay, `gen_translated --check`, `ledger.py --check`. INF / OPEN as marked.

## 1. Paired refcount rule (ruling item 1): DONE, committed

- The AddRef callee 0x1403DB250 is recognised by its bytes (`b8 01 00 00 00 f0 0f c1 01 c3`). An AddRef in the non-null region is admitted only when the object it increments (the object in rcx at the call) is one the region stored into the packet. Object identity is tracked: a register the region has not written holds the object it held on entry; `mov`, `lea` of a field address and `add reg,imm` keep it; a write gives a fresh token. Any other write to memory outside the packet still fails.
- Tests (20/20): paired passes (synthetic); unpaired AddRef fails; AddRef of an object the arm did not store fails; a packet slot read without a store fails; the single exit past a null-only block passes; a branch into a null-only block that writes outside the packet fails; the real closure function of 1408BE3C0 passes.
- Mutation (on a scratch copy, edit tool, no sed): pairing check disabled, 3 tests red.

## 2. 1408BE3C0 (ruling item 2): PROVEN on its six captures, NOT merged

- Re-proof on pid950952 seq002/004/006/008/010/012 (copies out of live2, removed): `calls=6 replayed=6 mismatches=0 own_block 23/55 (0.418)`, proven=True, with the boundary entry (sites 0x14195BA9E, 0x1419DBC0C, 0x1419DC774, 0x1419DC9ED, 0x1419DCFB5). The earlier mismatch was PES_RO_OUT_OF_WINDOW on the 0x142B67EF0 table (the walk reads 17 words, not one; corrected).
- Not committed: the merged record's closure pulls in 0x1419DB1D0, 0x1419DB2E0 and 0x141AD9220, which read 11 undeclared read-only image addresses (0x142A95698..0x142AB6B90). `gen_translated` refuses that, so the record is held out. Decoding those tables is the next step; the record diff is not kept, the proof is reproducible from the brief's captures.

## 3. 1408BED00 (ruling item 3): windows declared, proof still FAILS the guard

- Row counts (CONFIRMED, one entry, replay of pid1845381 seq7492 with the trace layer): entry to 0x141EBBDB0 has count(O1)=31 and count(O2)=20, where count(O) = dword [[O+0x100]+0x20]+0xF8 (0x141AFF0D0). The boot captures never reach 0x141EBBDB0.
- Declared in ro_tables.json (evidence per window, INF capacities from table spacing): 0x142B67F40 (0x200), 0x142B68140 (0x200), 0x142B68340 (0x280), 0x142B68480 (0x280), 0x142B685C0 (0x280), 0x142B68700 (0x140), 0x142B68840 (0x140), 0x142B68980 (0x140). Row counts 31 and 20 fit inside those capacities. One row count is one observation, so the capacities stay INF.
- The 1408BED00 proof then fails the guard: 0x1419DC350 site 0x1419DC36F: after the paired AddRef, the arm executes `xchg dword ptr [rbx+0x2c], eax` (a flag store into the paired object itself). That is a write to the object, outside the packet and outside the ruling. Pinned by a test. Owner decision needed: admit that store (a named sub-boundary), or leave 1408BED00 unproven. I did not widen the rule.
- Not translated: 1408BED00 is not proven, so nothing merged.

## 4. Replay-start captures (item 4) and the bind caller

- pid191227 (Main's rig): only `rec_140A8ED70` captures exist (seq439, seq119069, seq234925). The replay-start hooks (`1423E0170`, `1423E9050`, `140A8EE20`) have never fired, per Main.
- The reset `140A8ED70` (CONFIRMED): no direct caller; reached through vtable slot +8 of the UnitReplay vtable. The three captures return to 0x14042E18E, in match-step function 0x14042E120 (called only from 0x14042F0E7 in 0x14042F000). The step calls the reset through helper 0x1404653F0 (loads `[mgr+0x6A8]`, tail-jumps to its slot +8), gated by a flag. So the 41 calls are the per-step path, not a replay.
- The bind `140A8EE20` (CONFIRMED vtable slot +0x38 of UnitReplay): a loop in 0x142175540 calls `[elem+0x38]` on each element of an array passed in rdi (`rdx = (bp<<5) + [rdi+0x38]`). 0x142175540's only direct caller is 0x142174264, inside state machine 0x142174140 (six states; state 2 calls `[rdi+0x38]`). INF: the bind is reached through 0x142175540 from 0x142174140. Not confirmed: the array's element class.
- The goal-replay frame (Main's pid1267009 seq53249, copied, read, removed; CONFIRMED): `replay_frame_14217DEF0` returns into 0x142175540 at 0x1421755D9 (the loop body's `call [rax+0x38]` return). So the goal replay runs through the same loop, on a non-null element. This is the best lead for the bind: the goal path is 0x142174140 -> 0x142174264 -> 0x142175540 -> [elem+0x38]. Not confirmed which slot-+0x38 target that is in the goal replay (the element's class is INF).

## 5. Layer, checks

- `gen_translated.py --image pes_full.bin`: regenerated (10232 functions, 3832 recorded bit-exact, closure 6400). `--check`: byte-identical, rc 0. The regenerated change: pes_fn_141EB6840.cpp (the 0x142B67EF0 window size).
- `ledger.py --check`: current (44479 rows).
- Python: test_boundary_guard 20/20, test_pdata_cave 5/5.
- C++ test binaries: NOT rebuilt this round. The layer change is a constant array size in one closure file; no test target was run after it. Main's gate must build and run it.

## 6. Open items (numbered)

1. 1408BE3C0: declare the tables read by 0x1419DB1D0, 0x1419DB2E0 and 0x141AD9220 (0x142A95698..0x142AB6B90, 11 reads) with CONFIRMED evidence, then merge the record (section 2).
2. 1408BED00: owner ruling on the `xchg [rbx+0x2c]` flag store into the paired object (section 3). Without it, 1408BED00 stays unproven.
3. Row-count capacities for 0x141EBBDB0's tables are INF; a capture with a larger count, or the table bounds, would confirm them.
4. Bind 140A8EE20: confirm the element class of the 0x142175540 loop and its slot-+0x38 target on a goal replay (the goal-replay captures seq53249..53251 in pid1267009 are the source; seq53249 read here).
5. Replay-start hooks (1423E0170, 1423E9050, 140A8EE20) have not fired on 6.5 h of play with 11 goals (Main). The goal replay reaches the layer through 14217DEF0 from 0x142175540, not those hooks.
6. Slice 3 and slice 4 not started. Cursor advance not decoded.
7. The `commit` namespace cannot see this worktree (extern/godot-cpp symlink); committed by path-limited git.
8. Pre-existing: a duplicate `0x142581374` entry in ro_tables.json (two identical rows; left as found).
