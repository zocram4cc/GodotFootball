# OneWorld report (through cut 3, on OneWorld at bb373d3f)

Branch `OneWorld` on origin: `87311b80` (cut 1), `18f3a2d8` (trap oracle fix), `24c4e030` (No-Mod launch list), `f48f14ce` (cut 2), `9819e9db`, `dc534cba` and `bb373d3f` (cut 3). Pushed fast-forward: origin/OneWorld f48f14ce..bb373d3f (`git ls-remote` confirms bb373d3f07c151c0d800cfc56bafaee6c52bfed8). Scoping: `docs/ports/pes-design-OneWorld.md`. Decisions: D1 live matches on booted globals; D2 Linux only; D3 template globals written once (cut 0 classification); D4 cut 0, cut 1, cut 2, then stop for review before cut 3; D5 cut 3 reviewed and accepted by Main; cut 4 is the next session's first task.

## Cut 0 (measurement only; accepted)

Image `pes_full.bin` (462,929,920 B). Anonymous RSS. Fox world construction +786 MB (the heap arena was value-initialised), Boot +16 MB, LoadBody +51 MB, 22 players +2 MB. Boot write set 43,797 qwords, 43,727 in the zeroed tail. Two boots give the same image (hash equal). The six trap-side globals that PES does not write per call: motion-table 0x1436F23C0, anime-manager 0x1436F2378, registry 0x1437F4808, eyes-target 0x1436F2648, pad-manager 0x1437F0A58, clip-database 0x1437CEAA0 (template); G-world 0x1436F3DC8 is per match. LoadBody reverts 93 boot qwords: PES's own static-initialiser objects (list and hash heads), identical after LoadBody in a fresh boot. The 4.9 MB of loader segments are hand-built by GF (`BuildTrapLoaderData`, `pestrapdata.cpp:128-236`), a stand-in for PES's boot-time build: cut-4 list.

## Cut 1 (`87311b80`): one memory class and `PesBoot(memory)`, behaviour identical

`PesMemory` owns the image mapping, PES's heap, the stack and TEB, the OS arena; `PesBoot(memory)` runs the boot. Boundary (agreed): the trap world's heap, stack and window stay in `PesWorld` until cut 3. Gate: switch-off `c2016bf08fd0bc98`, pes-loop `da3b1eff7174bc8b` unchanged; 192 subset 191/192 (known kick-off item).

## Trap oracle on real captures (`18f3a2d8`, `24c4e030`)

Captures (all hooked at the trap entry 0x14079A470 unless noted; dumps read in place from live2):
- kick-off: pid2286718 seq001..024 (24, 02:47 launch).
- open play, first set: pid2523621 seq001..008 (8, 03:54 launch; `MI+0x116C == 1`, every 3rd call).
- in play: pid2571173 seq001..024 (24, 04:07 launch, every call).
- tick 0x140797CD0 (`traptick_140797CD0`): pid2523621, 14 captures (every 20th call).

Pipeline (this worktree's tools): `frame_trace.py recorded --hooks --spec-dir --out` per capture; `snapdiff.py --only ENTRY` over the set (spec `specs/core` for the trap, `specs/trapaction` for the tick); `trap_oracle.py` per capture. Snapdiff replayed every trap call with 0 mismatches (proven).

Two test bugs were stopping the oracle on real data (its SetUp and layout are unchanged):
1. `Build` copied the pad mask into `segments.back()`, which since `AddTo` is the anime chain's segment: a segfault on every capture. Now the loader's segment, by VA (`18f3a2d8`).
2. `TheMatchsOwnMemoryCommitsPesClipOnTheIssuedCaptures` named the old captures seq4469 and seq23906. Now: on every issued capture the match's committed clip equals the flat run's clip (PES's bytes), and at least one issued capture is required (`18f3a2d8`).
3. `TheCapturesCoverTheStartAndBothMirrors` counted only the pid3741448 launch as No-Mod (deleted at the 06-11 handover). Now a list of No-Mod launches, each cited: pid3741448 (01-11 ruling) and pid2523621 (config.ini `favorite_gameplay_mod = No Mod`, unchanged since 30-10 14:34; checked by Main 15-11) (`24c4e030`).

Results, official binary built from `24c4e030` (`87311b80` + the test fixes):
- kind 0, 56 captures: 9 of 11 `TrapOracle` tests pass on real data (the flat image equals PES on every captured call; derived P, rate ceiling, G, locomotion, motion table, action table, WriteActorP). Two fail, both coverage: `TheMatchsOwnMemoryCommitsPesClipOnTheIssuedCaptures` (no capture issues; rax is never 1 in the 56: 0x1425eac00, 0x1c5fee00, 0x10f0ee00, 0x1c8fee00, 0x1c9fee00, 0x1cafee00; the two old issued captures are gone), and `EachWindowBindingChangesTheCall` (`man A velocity x` is never read).
- tick, 14 captures: before the No-Mod list, `EveryCapturedTickOnGfsWindowEqualsPes`, `ListWhatTheOverlaySupplies`, `LoaderDataTheTickReadsEqualsTheCapture` pass; `TheCapturesCoverTheStartAndBothMirrors` counted 0 (fixed by `24c4e030`). The official run on `24c4e030`'s binary: 4 of 4 pass (`trapopen/official_ticks_24c4e030.log`).
- Skipped (no captures reachable; no check): `pes_teampass_oracle_test`, `pes_angle_oracle_test`, `pes_kick_chooser_oracle_test`, `pes_kick_header_oracle_test`.

Issued-trap state (static decode, `docs/re/pes-re-trap-kind0.md` section 2; reach CONFIRMED): entry registers: man = rdx (P; x f32 at +0x8, z f32 at +0x10), ball = r8. Issue needs: reach (the ball within 6 m, squared xz <= 36.0f, at the t0+1 sample or sample 0; both > 36 refuses with status 1), ball height class not 6, no teammate winning the race (FUN_14079FBC0, meaning INF), kind-3 bearing within 50 deg, and the action admits class 7. The reach test is float arithmetic on callee-computed samples: not expressible in `--when` (integer predicates only). The live distance at issue was never measured (`pes-re-trap-contact.md` section 5).

## Cut 2 (`f48f14ce`): the booted anime world is a frozen template; each match's world is a copy-on-write clone

`PesMemory::Freeze()` writes each region's dirty pages to a sparse memfd (the image over the install's file; the heap page and arena; stack; TEB; OS arena), and keeps the heap's bookkeeping. A clone maps each region's base (file or zeros) and lays the frozen pages over it, copy-on-write. `AnimeWorldTemplate()` boots, loads and freezes once per process per image, install and heap size; `Match::PesAnimeWorldOf` makes each match's world a clone. `PesHeap` maps its page and arena (zeroed lazily).

Measured (15-11-26; machine load 25 to 96; timings noisy; anonymous RSS):
| step | cut 0 | cut 2 |
|---|---|---|
| fox world construction | 0.07 s, +786 MB | 0.001 s, +2 MB |
| Boot | 0.03 s, +16 MB | 0.04 s, +16 MB |
| LoadBody | 0.75 s, +51 MB | 0.88 s, +84 MB (arena committed as touched) |
| anonymous RSS after Boot and LoadBody | ~900 MB | ~150 MB |
| template build (once per process) | - | 1.09 s; dirty 67.7 MB (image 0.74, heap arena 31.8, OS arena 33.5, page/stack/TEB 8 KB each); transient peak RSS 1.09 GB |
| clone | - | construction 0.001 s; +0.5 MB anon, +1.2 MB shared; 2 players x 3 frames 0.005 to 0.012 s |

Gate: switch-off `c2016bf08fd0bc98` and pes-loop `da3b1eff7174bc8b` unchanged; the world-building test binaries pass except the trap oracle's coverage items and the known kick-off item; the 192 subset 191/192 (the known `MoveStepWire.TheKickoffTakerClosesToTheBall`). `tests/pes_fox_template_test.cpp` (3 tests) passes: a clone holds the fresh boot's bytes on every region and the same bookkeeping; frames are bit-identical; a running clone leaves another clone and the template unchanged.

## Cut 3 (`9819e9db`, fixes `dc534cba` and `bb373d3f`; on `OneWorld`; reviewed and accepted by Main)

One PES memory per match. `PesMemory` holds the image mapping, heap, stack, TEB, OS arena and the match's trap window (owned by the memory), and shared regions (the loader's data and the anime chain's layers, frozen into the template and borrowed by every trap call). The template writes the loader's pointer globals except the clip database (the booted ResManager stays, as PES had it). The match's trap world runs on the anime engine's memory (`BindPesTrapWorld`, pes_loop only).

Bug, found on the pes-loop smoke (rc 134 at the first 5 s stalemate): the window moved at `BindMemory`, but the period clock, built earlier on the host window, held a `PeriodRecord&` into the deleted copy. Every later write went to freed memory, so the window's period record stayed zero (the first judge and tick lines printed period 0, checksum 0, ring 0; the check failed on all 1680 printed lines). The judge's checksum check (`0x140A3C070`, reached through `TripFromContact` -> `0x1405B7470` -> `0x140A74DB0` -> `0x140A3C050`) failed on that record, and the first call that reached the cold branch aborted the run. Classification: A, a port bug of this cut.

Fix (`dc534cba`): the trap world binds before the clock is built. Evidence from a removable diagnostic (not committed; `gf-AnimeLayer-scratch/cut3_fix_with_diag.patch`): the fixed run has 29,320 diagnostic lines, 127 of them at judge calls, and none fails GF's transcription of PES's FNV check or GF's `PeriodStamp`; the run completes with no PES abort. The transcription is GF's code, so PES's own check is evidenced by the absence of its abort, not by the diagnostic.

Smokes (seed 7, 2 min): switch-off `c2016bf08fd0bc98` (unchanged); pes-loop `da3b1eff7174bc8b` (unchanged from cut 2, so no re-baseline: the trap now runs on booted globals and one memory per match, and this match's digest does not move).

Gate on `bb373d3f` (the final code; full build, then `cut3_gate.sh`; summary `gf-AnimeLayer-scratch/cut3_summary.txt`): switch-off `c2016bf08fd0bc98` and pes-loop `da3b1eff7174bc8b` unchanged. 28 of 30 test binaries return 0. The 192 subset: 191 of 192; the one failure is `MoveStepWire.TheKickoffTakerClosesToTheBall` (the known kick-off item, #1289). `pes_fox_template_test` 3 of 3. Two binaries fail for reasons outside cut 3: `pes_movestep_wire_test` (the same kick-off item) and `pes_trap_oracle_test`: its 4 `TrapTickOracle` tests find no dumps (the tick captures in `ticks2/oracle` are packed, so their `mem.bin` links are broken), and its kind-0 `TrapOracle` tests skip (`GF_PES_TRAP_ORACLE` is not set in the gate; the explicit runs below are the kind-0 evidence). The tick oracle passed 4 of 4 on `24c4e030`'s binary with its dumps unpacked.
The template reference fix (`bb373d3f`): the cut-3 template writes the trap loader's pointer globals (`AddTrapLoaderData`), so the fox template test's fresh-boot reference had to carry the same loader data. Each reference boot now calls the helper; without it the clone's image differed in those cells, and two of the three tests failed on the image alone.
Oracle, reach-gated set (live2 pid3727195: seq025-037, seq050, seq063, seq064; 16 captures in open play): one issues (seq037, rax 0x1); the other 15 refuse. Official kind-0 binary (`24c4e030`): the 16 alone 10 of 11; the union with the 56 (72) 10 of 11. `TheMatchsOwnMemoryCommitsPesClipOnTheIssuedCaptures` passes now (on seq037 the match's committed clip equals PES's). `EachWindowBindingChangesTheCall` leaves one binding unread on the 72: `man A velocity x` (`+0x574`). Open coverage item. The doc puts a read on the kind-3 arm (`pes-re-trap-kind0.md` section 2 step 8: `query+0x6C - 3 == 0`, bearing within 50 deg), but the race refusal (section 3, line 50) also reads a presser's `+0x574` on calls that reach its loop, and `man A` is the first live-in man of the call's manifest, not necessarily the presser. So "kind-3 only" is not established. Logs: `trapopen4/official_16.log`, `trapopen4/official_72.log`.

Oracle rerun recipe: open item 4.

Not closed by the fix: E1 (open item 2). The original run, the clip-database experiment, E2 and E4 all moved the window and left the clock on the deleted copy, so the dangling reference explains them. E1 is the exception. It kept the window on the host, so the clock's record and the judge's were the same bytes, and it still aborted (rc 134). E1 also still had the clip-database write in (that experiment was not reverted before E1). Its abort site was not captured. E1 and the fixed tree differ in two things: the clip-database write and the window move. Commit `9819e9db`'s message says all experiments "still abort at the same site" and calls them excluded: that overstates. Only the original run (core dump PID 3718460) and E4 (PID 3754482) had their sites checked, both in the judge path. This paragraph supersedes that message; the commit itself is unchanged, because `bb373d3f` is the tip Main accepted.


## Open

1. Clobber audit (not run). Compare a clone's image after a pes-loop match with the template, and attribute each differing range to a trap-world or fox-world write. It checks the claim that no trap write lands on fox state.
2. E1 (open; the one abort the fix does not explain). Decisive run, not done: on the fixed tree, restore the clip-database write (drop the `kClipDatabaseGlobalVa` exclusion in `AddTrapLoaderData`), keep the window move, run the pes-loop smoke, and capture the abort site (core and gdb) if it aborts. If it aborts, the clip-database write is the cause and the exclusion stays. If it passes, E1's no-move variant must be explained before it is set aside. The diagnostic patch is `gf-AnimeLayer-scratch/cut3_fix_with_diag.patch`.
3. `man A velocity x` (`+0x574`): open coverage item. No call in the 72 reads it. The binding names the first man row of the call's manifest, which need not be the presser. The doc puts the reads on the kind-3 arm (`pes-re-trap-kind0.md` section 2 step 8) and on the race loop, which reads the presser's record through `FUN_140A48A10(G, id)` (section 3). Closing it needs a capture on the kind-3 arm, or a binding on the presser's record.
4. Oracle rerun recipe. The oracle directories link each capture's `mem.bin`, and `tools/dev/rendezvous/capz.py` packs those files. (a) Check space: `python3 tools/dev/rendezvous/capz.py status` on `<workdrive>/gf-Rendezvous-scratch/live2/pid2286718`, `pid2523621`, `pid2571173` and `pid3727195` (about 4 GB per unpacked capture; unpacking a launch directory unpacks every capture in it). (b) `capz.py unpack` on the same four directories. (c) Run the official kind-0 binary built from `24c4e030`, `<workdrive>/gf-AnimeLayer-scratch/trapopen/pes_trap_oracle_test.nomod`, with `PES_EXE=<local>/ghidra-projects/scratch/Unpacker/pes_full.bin PES21_ROOT=<workdrive>/PES21 GF_PES_TRAP_ORACLE=<workdrive>/gf-AnimeLayer-scratch/trapopen4/set72 <workdrive>/gf-AnimeLayer-scratch/trapopen/pes_trap_oracle_test.nomod --gtest_filter='TrapOracle.*'`. `set72` is the 72 oracle directories; `trapopen_all56/` is the 56; `trapopen4/set16/` is the 16. The tick suite takes `GF_PES_TRAP_TICK_ORACLE=<workdrive>/gf-AnimeLayer-scratch/ticks2/oracle` (pid2523621). (d) `capz.py pack` on the same four directories afterwards.
5. Cut 4 (next session's first task): close the BodyAnimControl, DemoInfoRef and press planner-object seams, and the hand-built loader segments.
6. Day file (`tasks/14-11-26.md`): not updated (Main's file).
