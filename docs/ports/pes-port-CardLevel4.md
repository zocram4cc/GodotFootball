# PES port CardLevel4: the two red judge cases were a translator defect (switch tables never baked, so the GF runtime read 0 and 0x140A63DA0 always answered "no ball played"); fixed in xlate, 82 layer files regenerated; judge decide tests 7/7 green on the translated judge; cutover folded into FoulSanction (30-10-26)

Branches (confirmed by `git ls-remote`): `FoulSanction` and `CardLevel2Cutover` both `f1f2b2f8` (FoulSanction = the cutover; the hand-port restore `1a7c7adf` is kept in history by a `-s ours` merge, its content superseded). Worktree `<workdrive>/gf-FoulSanction`, scratch `<workdrive>/gf-FoulSanction-scratch`. CONFIRMED = tool output this session; INF = inference; OPEN = blocker named.

## 0. Status against the ticket

| item | status |
|---|---|
| 1. merge `origin/main` into both branches, push | DONE. Merged on `CardLevel2Cutover` (`2804d7df`): the only hand conflicts were `closure.py` (took main's `pdata`-based rewrite whole; the leaf-widening it replaces is main's `Pdata`) and `gen_translated.py` `SETS` (stoppage + fixdemo both kept); `test_closure_region.py` deleted (main deleted it; `test_extent.py` and `test_closure_walk.py` replace it, 19/19 and 18/18 OK); the 94 conflicting generated files were not hand-merged: the layer was regenerated on the merged tree (4,104 functions, `--check` byte-identical). FoulSanction took the same tree by `merge -s ours origin/FoulSanction` on the cutover, then fast-forwarded |
| 2. the two red cases | DONE, root cause below: neither the facts nor the expectations were wrong; the translated code was emitted without its jump tables |
| 3. fold into FoulSanction, mutations, delete hand port | folded; 9 mutations run (section 3); the hand-port scoring (`JudgePoints`/`JudgeLevel`) is gone, `pesjudge.cpp` keeps only the advantage/offside constants (not the foul scorer) |
| 4. replay new `judge_commit` (level >= 2) / `judge_decide` captures | NONE ARRIVED. `live2/pid483669` (the PES launch of 02:43; the brief's "483650" is the process name stem, the capture folder is 483669) holds no judge_commit/judge_decide capture; the watcher log's last judge_decide is still pid132854 seq5123 (below threshold) and its last commit pid2670420 seq7443. PES was restarted again (pid 615629, 49 hooks, KickKind) and its `kick_chooser` releases are failing on "disk under 400 GiB". Card proof (OPEN item 1 of CardLevel3) stays open |
| 5. smoke + fast gate | see section 5 |

## 1. The diagnosis (CONFIRMED)

Trace (gdb on `pes_judge_decide_test`, breakpoints on the `pes_fn_*` callees, return value read from `PesState::gpr[0]` at each finish; script `gf-FoulSanction-scratch/trace.py`): in `AProfessionalFoulIsAtLeastAYellow` the judge `0x1405B7470` calls `0x140A63F60 -> 0x140A63DA0(B=3)` and gets 0 ("B did not play the ball"), then `0x140A639A0` = 1, so it takes the step-2 branch (FORCE foul only: `0x1405B7470` `if (cVar6==0 && !bVar5)`), reads `ai(B)+0x14A0 != A` and returns. The staged bytes were right: B's anime record (VA `0x200010EC18`) holds action `0x0D` and flag word `0x8000` (bit 15), `+0x44` = 0.

Watching `PesState::gpr[3]` (BL) inside `0x140A63DA0`: BL is cleared and never set. Its action dispatch is the two-level MSVC switch (`0x140A63DDC..0x140A63E02`): `lea eax,[rdx-8]; cmp eax,0x35; ja default; lea r8,[0x140000000]; cdqe; movzx eax,byte[r8+rax+0xA63F20]; mov ecx,[r8+rax*4+0xA63F10]; add rcx,r8; jmp rcx`. Tables read from the image: byte map `0xA63F20` (action 0xB..0xE -> 1, 0x38.. -> 2), dword table `0xA63F10` (`0x140A63EBF, 0x140A63E04, 0x140A63E32, 0x140A63ED9`). The generated code did `s.rd(0x140000000 + rax + 0xA63F20, 1)`: a live image read. The GF runtime has no image view (`NewCall` maps only the world window), `PesMem::rd` returns 0 on a miss, so both loads read 0, `_i = 0`, and every action fell to the default case (`0x140A63ED9`: flag 16 test, 0). The translator bakes a register-indexed image read only when `bounded_index` finds the guard that bounds the index; here the CDQE (no operands) hides the guard, and the second index is a byte-map value, not a guarded register. The snapshot harness maps the whole image flat, so every diff and proof (3,693 recorded bit-exact) stayed bit-exact while the same code was wrong at GF runtime.

Consequence (INF from the same pattern, not each exercised): the scorer `0x1405B6D40` and the contact rule `0x140A639A0` also had switch loads without `ro_rd`, so the points were 0 and the judge decided "no foul" whatever was staged. 74 of the 123 layer functions with a `switch (_i)` carried no baked table; 82 files change.

## 2. The fix (translator emission, not decode; no translated file edited by hand)

`tools/dev/translate/xlate.py`: `prepass` records the windows each resolved JMP-reg switch itself reads (`"tabs": {byte map VA: count, dword table VA: n*4}`); `emit_function` hands them to `Emitter.sw_tabs`; `Emitter.bake_ro` falls back to them when `bounded_index` finds no guard (guard-bounded reads keep their old span, so existing baked output is unchanged). The decode (`capstone_decode`, the int3 guard) is untouched.

Red test first: `tools/dev/translate/test_switch_tables.py` (synthetic image with a `.text` section, a CDQE + byte map + dword table switch, translated, compiled with g++ and run with NO image view). On the old `xlate.py`: 2 of 3 fail (actions 8..10 and 11..13 answer 0 instead of 0x11/0x22). On the fix: 3/3. 82 layer files regenerated (`gen_translated.py` 4,104 functions, `--check` byte-identical); `test_extent` 19, `test_closure_walk` 18, `test_setcc_store` all OK.

## 3. Tests and mutations (CONFIRMED, each rebuilt, run, reverted)

`pes_judge_decide_test` on the translated judge: **7/7** (5 original, 2 added: `ANoBallContactAimedAtTheManIsAForceFoul` expects pending level 1 type 8, `ANoBallContactNotAimedAtTheManIsNoFoul`). `pes_judge_test` 12/12. The two originally red cases needed no change.

The first test's claim was wrong and is rewritten from the decode: a tackle that never played the ball is a challenge whose "no play" `0x140A63DA0` reports through its out flag (`*param_3 = 1` at `0x140A63EEE`), so the contact rule `0x140A639A0` refuses it and no force target matters. It is renamed `ATackleThatNeverPlayedTheBallIsNoFoul`; the force-target rule is now tested with a no-challenge action (action 0).

| mutation (pesworldrecord.cpp `DecideContact`/`DecideJudge`) | killed by |
|---|---|
| M1 victim never staged as fallen | AProfessionalFoul, SameTackleWithoutIntent |
| M2 intent flag (`+0x134`) never set | AProfessionalFoul |
| M3 played-ball flag never staged | AProfessionalFoul, SameTackleWithoutIntent |
| M3b flags 15/16 swapped for a challenge | AProfessionalFoul, SameTackleWithoutIntent |
| M4 face flag (bit 29) never set | TheFaceFlagOutsideTheBoxIsAYellow |
| M5 force target always the victim | ANoBallContactNotAimedAtTheManIsNoFoul (SURVIVED the 5 original tests; the added pair kills it) |
| M5b force target never staged | ANoBallContactAimedAtTheManIsAForceFoul |
| M6 staged action bytes not restored | TheStagedActionBytesAreRestored |
| M7 fouler register carries the victim | ForceFoul, AProfessionalFoul, SameTackleWithoutIntent |
| M8 judge ctx cell (`judge+0x3B8`) unbound | **SURVIVES** all 7 (OPEN item 2) |

## 4. Open items (numbered)

1. Card proof: no `judge_commit` level >= 2 or `judge_decide` that reaches the pending-foul builder `0x1405B8000` has been captured (section 0 item 4). Ask Main to keep the `--when` predicates in the allowlist after the KickKind restart (49 hooks) and to free disk (`kick_chooser` releases on "disk under 400 GiB"); replay with `tr.sh` + `sd.sh` in the scratch dir.
2. M8 survives: no test depends on A1 (the judge's ctx cell, `A1+0xE0` the ball-event ring). The judge reads "A made the newest ball event" (step 3 event switch, step 5 else-if "a contact on a man who just played the ball is at least a foul") through it, and GF stages no ball event there at all (`JudgeContact` writes none). Staging the ring and a test per event kind needs the ring/event layout decode (`0x140A3E190`, event `+0x10` frame, `+4` actor).
3. The translator class behind the bug is wider than switches: any register-indexed read of a read-only image table that `bounded_index` cannot bound is still emitted as a live `s.rd` that returns 0 at GF runtime. A generator lint (fail the generation when an emitted `s.rd` address can fall in a non-writable image section) would find them; not written.
4. Every `judge`-adjacent proof (`results_judge.json`, decide 4/4, commit 1/1, whistle 28/28) was taken before this regeneration; the harness image is flat so the baked bytes equal the live ones and the regenerated bodies read the same values. Not re-run (the `gen_translated.py --check` is the drift guard).
5. `xlate.capstone_decode` int3 tails (CardLevel3 item 3) still a separate reviewed change; not touched.

## 5. Checks run

- Built: `pes_judge_decide_test`, `pes_judge_test` (full layer, `-O0 -DNDEBUG`). Ran: `pes_judge_decide_test` 7/7, `pes_judge_test` 12/12, `test_switch_tables.py` 3/3 (red 2/3 before), `test_extent.py`, `test_closure_walk.py`, `test_setcc_store.py` OK, `gen_translated.py --check` clean after the merge (4,104 functions) and after the fix (regenerated, committed).
- Fast gate and smoke: see the lines at the end of the final message.

## 6. Smoke and gate (CONFIRMED from the gate's own log `gf-FoulSanction-scratch/gate.log`, `build/gate.smoke-*.log`)

- **`GATE_TIER=fast GATE_SLOTS="3 2"` on `f1f2b2f8` did not finish (my 3600 s cap killed it; no final line). Interim: `smoke-off rc=0` (2 min, `[ticks] ... sim ticks to full time`), `smoke-pesloop rc=134` ABORT.** The abort is a libstdc++ assertion: `std::array<int,64>::operator[]` out of range called from `pes::movestep::RunLoop` (`ProcessMoveStep <- Player::Process <- Team::Process <- Match::Process`), at the first `[stalemate] 6.2s idle` line of the 2-minute `--seed 7 --pes-loop` run (gdb backtrace: `smoke_gdb.log`). The array is `styleGroup` (`styleGroup[style*4 + member/4]`, `movestep_run.cpp:449`, `:2452`) or `kindBandRow[kind + band*8]` (`movestep_step.cpp:754,1494,1602`, `movestep_group_a.cpp:232`): an index >= 64 means a `style` >= 16 or a `kind/band` combination out of range. `src/pes/movestep*` is byte-identical to `origin/main` (no diff), and the pes-loop smoke passed on Main's AO/AP gates (`gf-MergeLane-gateAO.log`, `gateAP.log`: `smoke-pesloop rc=0`). So the abort is exposed by something this branch changes: the translated judge now decides contacts (Referee), or the 82 regenerated layer files (switch tables now dispatch where they used to fall to default, in the AI/animation/gameplay/cutscene functions the pes-loop runs). NOT bisected: a second gdb run to read the index (`--minutes 1`) hung for the 1300 s cap, and a full rebuild at the pre-fix commit `2804d7df` (merge only) costs about 40 minutes. The next step: build `2804d7df` (judge cutover, no table bake) and `f1f2b2f8` and run the same 2-minute `--pes-loop` smoke on each; if only `f1f2b2f8` aborts, print the style/kind/band index in `RunLoop` and find which regenerated function feeds it (a function that used to answer the default case now answers PES's real case, and GF's `LocomotionStyle` input range is wrong for it).
- The `smoke-off` path (no `--pes-loop`): 2 min, finished, `rc=0`. Determinism (same hash on rerun) not run: the gate stopped at the abort.
- Branches `FoulSanction` and `CardLevel2Cutover` are therefore NOT safe to merge until the pes-loop abort is explained; the judge decide tests (7/7), `pes_judge_test` (12/12) and the translator tests are green.
