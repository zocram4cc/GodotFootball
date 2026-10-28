# PES port FoulOffside4: the involvement test follows PES's rule (decoded); sanction A1 layout CONFIRMED; sanction switch and cutover NOT done; first-tick crash fixed (`6ed1a32b`)

Branch `FoulOffside`, worktree `<workdrive>/gf-FoulOffside`. Pushed: `b00f6a7e` (involvement test) and `6ed1a32b` (crash fix); `git ls-remote origin FoulOffside` = `6ed1a32b`. Labels: CONFIRMED = read from code, a test run, or a capture this pass; INF = inference (reason given); OPEN = blocker named. Scratch (PES-derived, not committed): `<workdrive>/gf-FoulOffside-scratch/` (`a1_decode.py`, `stop_scan.py`, `ring_decode.py`, `seed_runs.sh`).

## 0. Status per ticket item

| # | Item | Status |
|---|---|---|
| 1 | Red involvement test; decode `0x140A3A500` | **Done.** PES involves the 35 m assistant. The expectation was GF's assumption. Test corrected to PES's rule; GF's inputs were already right. Pushed `b00f6a7e`. |
| 2 | Sanction `0x14047F170`: A1 layout, build A1, call from referee | **Layout CONFIRMED** (§3). **Switch NOT done** (§4 blockers). No red sanction test. |
| 3 | Clean cutover of `PublishPesStoppage` | **NOT done** (§5). Scope measured; not started to avoid a half-state. |
| 4 | Mutations and seeds 7 and 3 `--pes-loop` | **Involvement mutation: done** (§2). Sanction mutation: N/A (no switch). **Seeds: §6.** |
| 5 | `sed -i` slip | **Checked: consistent, nothing to fix** (§7). |

## 1. Involvement: PES's rule and the test (item 1)

**Helper `0x140A3A500`, stopped branch (CONFIRMED by the translated body `src/pes/translated/impure_sample/pes_fn_140A3A500.cpp`, lines 79–157):**

- Inputs: `rcx = A0` (the official's frame; `[A0+4]` = index), `dl` = a flag from the caller. `G = [0x1436F3DC8]`, `MI = [G+0x270]`. The official's man record comes from `0x140A48A10(G, index)`.
- `0x140A48A10` (CONFIRMED, `ai/pes_fn_140A48A10.cpp`): index ≥ 26 returns `[G+0x1DD8]`; otherwise it returns `[G + 40·(index+191)]`. `191·40 = 0x1DD8`, so the table entry is `G+0x1DD8+40·index`, the same 0x28 stride `PesWorldTables` uses (`pesworldrecord.hpp` "step 0x28 per index"). GF's `ManVa` binding agrees.
- Playing state is `MI+0x116C`. When it is not 1 (stopped), the spot is `(MI+0x155C, MI+0x1564)` = (x, z).
- Decision (CONFIRMED from the code):
  - `signX(spot) == signX(official)` is required (`sign` = 1.0 when the value is ≥ 0, else −1.0; compare at lines 128 and 143).
  - `signZ(spot) == signZ(official)` is required (line 143).
  - `|spot.z − official.z|` is computed (lines 146–153, double abs mask `k_c142565D30`), then compared with **14.0** (`k_c1425C48B8_f`, image constant at `0x1425C48B8`, bytes `0x41600000` = 14.0f). Involved iff `14.0 > |Δz|` (line 156).
  - **The distance along x (goal-line direction) is not an input.** The stopped branch compares z only.
- Flag `dl` (INF): the caller `0x140A3A690` sets `dl = (MI+0x1544 == 1)` on its paths (`impure_sample/pes_fn_140A3A690.cpp` lines ~106–127). A foul stop has `MI+0x1544 = 0`, so `dl = 0` and the Z-rule branch runs. The boundary tests below confirm that branch empirically.
- The referee (index 22) is involved on every stop via label 0 of `0x141E53520` (CONFIRMED, unchanged from FoulOffside3 §2.1).

**Verdict (CONFIRMED by decode and test):** PES **involves** the assistant 35 m from a same-half foul when his touchline offset is 0. The red expectation (`AnAssistantThirtyFiveMetresFromAFoulIsNotInvolved`) was GF's assumption that PES measures Euclidean distance. PES measures the z offset only. GF's inputs were **correct**: spot at `MI+0x155C/0x1564`, official at man `+0x55C/+0x564` through the table stride. No input fix was needed.

**Test changes (`tests/sim/pes_involvement_test.cpp`, commit `b00f6a7e`):**
- The 35 m test is now `AnAssistantThirtyFiveMetresDownTheLineIsStillInvolved` (expects true).
- New boundary pair on the 14 m rule, same X and Z sign: `AnAssistantThirteenMetresAcrossTheTouchlineIsInvolved` (|Δz| = 13, true) and `AnAssistantFifteenMetresAcrossTheTouchlineIsNotInvolved` (|Δz| = 15, false).
- Result (CONFIRMED, direct binary run): `pes_involvement_test` **6/6 PASSED**.

Red history (honest): before this change the 35 m test asserted `false` and the translated helper returned `true`. The FoulOffside3 doc's open item ("decode lines 79–156") is closed by §1.

## 2. Involvement mutation (item 4, one switch)

- Mutation: `PesWorld::Involved` writes `official + 1` into `[A0+4]` (`src/sim/onthepitch/pesworldrecord.cpp` line 64).
- Rebuilt `pes_involvement_test`, ran: **4 FAILED** (`TheRefereeIsInvolvedInAFoulWhateverTheDistance`, `AnAssistantFiveMetresFromAFoulOnHisHalfIsInvolved`, `AnAssistantThirtyFiveMetresDownTheLineIsStillInvolved`, `AnAssistantThirteenMetresAcrossTheTouchlineIsInvolved`); 2 passed (the 15 m and other-half cases, which are false either way).
- Reverted (edit tool), rebuilt, reran: **6/6 PASSED**. `git diff` on `pesworldrecord.cpp` is empty.

## 3. Sanction A1 layout (item 2): CONFIRMED from foul captures

**Correction of the RE doc.** `pes-re-sanction-caller.md` §6 and §8 item 1 say no capture has a foul stop. That is out of date. Scan of all 191 captures (`stop_scan.py`, reads `MI = [G+0x270]`, `stop = MI+0x1574`): stop-reason histogram {0: 94, 3: 25, 4: 10, 5: 25}. **25 captures are foul stops (`stop == 5`).** In every foul stop the foul kind (`MI+0x1588`) is 2 (direct) and card (`MI+0x158C`) is 1 or 3. **Twenty of them carry the sanction hooks** (`foul_sanction_47F170` ×10, `foul_sanction_caller_480910` ×10). The replay table in `pes-port-RefReplay3.md` §2 already proves 47F170 on these: 20/20 bit-exact.

**A1 (= the tree's `rdx`, `owner+0x228`) fields the tree reads (values read from the real captures, `a1_decode.py` and `ring_decode.py`):**

| A1 offset | Meaning | Evidence | Label |
|---|---|---|---|
| +0x00 | vtable `0x14259D590` | 5/5 sampled captures | CONFIRMED |
| +0x18 | pointer to block; `+0x24A0C` per-slot records via `0x14042D5E0` | pointer non-null in 5/5; `+0x24A1C` dword is 0 in the 5 sampled | pointer CONFIRMED; per-slot meaning OPEN (`0x14042D5E0` has no `.pdata`, not decoded) |
| +0x6C | 40 × 12-byte table, read by `0x140A81230` (header dword at +0) | layout from the decode in `pes-re-sanction-caller.md` §2.2 | layout CONFIRMED from disasm; meaning INF |
| +0xE0 | pointer to the match-history object E0 | non-null 5/5 | CONFIRMED |
| E0 + k·0x1908 | kind `k` block; count dword `+0x1900`, head dword `+0x1904` | kind 0 count = 8 in 5/5; kind 4 count saturates at 8 (seq060/061: count 8, head 1, so the ring wraps mod 8) | CONFIRMED |
| kind + slot·0x320 | record; newest = `slot = head mod 8` | 25 foul stops; rec[0] matches MI in every sampled stop (below) | CONFIRMED |

**Kind-4 record at a foul stop (CONFIRMED on the sampled foul stops; `ring_decode.py`):**
- `+0x00` = 30 (0x1E) in every sampled record. The tree's injury test `dword == 0x37` (`0x14047F364`) is **not** triggered by these foul records.
- `+0x18` = foul kind = `MI+0x1588` (2 in all 25 foul stops).
- `+0x1C` = card colour = `MI+0x158C` (1 or 3; equal in 9/9 sampled stops).
- `+0x28` = side = `MI+0x1558` (0 or 1; equal in 9/9 sampled stops).
- `+0x20` = 0 or 1, **not** the fouler or victim slot (fouler 1–17 and victim 1–21 are not stored in the record). Meaning OPEN.
- `+0x42` (injury byte) = 0 in all sampled records.

**Scene output (CONFIRMED from the decode in the RE doc):** the tree returns the scene id in `eax`. Its request record `A0` (`req = Flow+0x6D8`) takes it at `[req+0]`. GF has no Flow, no `req`, and no scene system.

## 4. Sanction switch: NOT done (blockers, item 2)

1. **No GF consumer for the scene id.** GF has no scene system. Per the ticket it goes to the block for the cutscene layer, but the PES offset is `req+0` (Flow-relative), which is not in the match-info block. Putting it in a GF-invented field would be an un-decoded PES offset: not done.
2. **The tree's direct callees include presentation** (`141794410` in-match menu, `140A74D00` and `140A489D0` cutscene, `140A73E20` HUD, plus `1414DAB90` and `140A3AF90`). All are translated bodies, but running them in GF's window drives PES's UI and cutscene state. Which of them must be stubbed (with a named `// ponytail:` PES site) is not decided. Stubbing them is a design call, not done.
3. **A1's owner is not decoded.** `A1 = owner+0x228` (RE doc §2.1). The owner's class and constructor are OPEN. Fields at `+0x18` (`0x14042D5E0` per-slot) and `+0x6C` (`0x140A81230` table meaning) are unknown, so a GF-built A1 cannot be verified against PES's behaviour beyond the replay.
4. **The GF hand code it replaces is not identified.** The tree does not set the hold: the wrapper `0x14047FEB0` writes `[req+8]` (start gate). GF's card and hold timing (`CheckFoul` / `ScheduleStop(kPesHoldFoul)`) is not replaced by anything the tree outputs (INF, RE doc §7.1).
5. **No red sanction test.** A test needs an expected value. The only oracle is PES's own output on captures (replay harness), and PES bytes must not be pinned in tests. No GF-side oracle exists yet.

## 5. Clean cutover of `Referee::PublishPesStoppage` (item 3): NOT done

- `src/sim/onthepitch/referee.cpp:771` writes 13 MI fields (restart kind, side, spot x/z, stop reason, foul kind, card level, fouler and victim slots, offside flag, armed flag, ball, attack signs) from `buffer`, `foul`, `offsidePlayers` and `prepared`. It is a mirror.
- The GF readers of those fields: about **247** text references (`restartPos`, `restartTeam`, `foul.foulType/foulPlayer/foulVictim/foulPosition`, `offsidePlayers`, `prepared`) across `src/sim`. Moving the writers changes `referee.hpp` (header), so most of gfsim rebuilds (about 18 min at -j3).
- Not started: a partial move would leave two states, which the ticket forbids.

## 6. Seeds (item 4): see the numbers below

Matrix: seeds 7 and 3, `--pes-loop --minutes 10`, `tools/dev/simslot.sh timeout 1500` (slots 2–3 with the gate on 0–1, `GF_SIM_SLOTS=4`).
- **FoulOffside** = this worktree's `gfsim_run` rebuilt at `6ed1a32b` (the fix in §6b; seeds 7 and 3 were run on it, 28-10-26).
- **main** = `build/apps/gfsim_run`, **built 21-10-26, not main HEAD** (a fresh main build did not fit the budget). It is a stale baseline; treat it as a labelled reference, not a like-for-like diff.

| run (`--pes-loop --minutes 10`) | restarts | fouls (sim line) | tackles / slides | yellow / red | sim ticks to full time | idle episodes / stalls |
|---|---|---|---|---|---|---|
| FoulOffside seed 7, fixed (`6ed1a32b`) | 27 | 5-3 | 2-1 / 7-2 | 0-0 / 0-0 | 58893 | 0 / 0 |
| FoulOffside seed 3, fixed (`6ed1a32b`) | 21 | 1-5 | 2-3 / 2-4 | 0-0 / 0-0 | 52602 | 5 / 5 |
| main seed 7 (stale binary, 21-10-26) | 22 | 3-4 | 0-3 / 6-6 | 0-0 / 0-0 | 71168 | 51 / 33 |
| main seed 3 (stale binary, 21-10-26) | 18 | 0-5 | 0-0 / 0-9 | 0-0 / 0-0 | 65624 | 42 / 25 |

**Read:** on these two seeds the cards are 0-0 in every run (yellow and red), so the card-count comparison is empty. Stoppage counts differ by seed (FO 27 and 21; main 22 and 18). No like-for-like conclusion is drawn from 2 seeds on a stale baseline. The FO runs show no idle stalls on seed 7 and 5 on seed 3; main shows 33 and 25. The FO "5-3" and "1-5" fouls are the sim's own counters.

**6b. Crash found and fixed (CONFIRMED).** The FoulOffside build before the fix (`b00f6a7e`) segfaulted on both seeds (`fo.seed7/3.log`, rc 139, no sim output). systemd core `core.gfsim_run…43877` (decompressed; `gdb`) puts the fault in `Referee::Process()+980`. That is the mirror (`PublishPesStoppage`, inlined) dereferencing `foul.foulVictim` on the first tick. `Referee::Referee` set `foulPlayer = nullptr` but never `foulVictim`, so the pointer was indeterminate. Fix: `foul.foulVictim = nullptr` in the constructor (`src/sim/onthepitch/referee.cpp:147`, commit `6ed1a32b`). A regression test for this was NOT written (it needs a match fixture; open item 7). The mirror is still in place until the cutover.

## 7. The `sed -i` slip (item 5)

Two constants (`kMiFouierOffset`, `kMiPlayedArmedOffset`) were renamed with `sed -i` in FoulOffside3. `grep -rn` over `src tests tools docs` finds **no** remaining old names. The rename is consistent. Nothing to fix.

## 8. Verification run this pass

- `pes_involvement_test`: 6/6 PASSED (after mutation revert; mutation rc table in §2).
- NOT COMPLETED. The 98-target `pes_*` build exceeded the 1800 s job limit under fleet load and was interrupted at link 75/99. No `pes_*` binary was run beyond `pes_involvement_test`. The earlier counts in FoulOffside3 §5.11 (`pes_match_info_test` 2/2, `pes_world_test` 6/6, `pes_team_tick_test` 26/26, `pes_clock_test` 42/42, `pes_mark_table_test` 16/16) are from the pre-merge FoulOffside3 tree and are not re-verified here.
- Gate: not run (Main gates).

## 9. Open items (numbered)

1. **Sanction switch** (§4 blockers 1–5): needs (a) a decision on stubbing the presentation callees, (b) the owner of A1 (`owner+0x228`) constructed in PES's layout, (c) `0x14042D5E0` and `0x140A81230` decoded, (d) a scene consumer or the cutscene-layer field in the block, (e) a PES-derived oracle for the red test (replay harness output, not a pinned number).
2. **Cutover** (§5): move the 13 MI-mirror writers into the block, retarget ~247 reads, delete `PublishPesStoppage`'s mirror. Header change; a full gfsim rebuild.
3. **`+0x20`** of the kind-4 record: meaning OPEN (not the fouler, not the victim slot; values 0/1).
4. **`dl` flag of `0x140A3A690`**: the Z-rule branch is confirmed by the boundary tests, but the outer control flow is INF from the translated body.
5. **Main baseline for the seeds**: a main build at `60617c23` (or `origin/main`) is needed for a like-for-like comparison.
6. **Involvement consumer**: the linesman's `holdForStoppage` and look-at `0x140A38F20` are still not switched (FoulOffside3 §5.6).
7. **Crash regression test** for §6b (a first-tick `Process()` with a fresh referee and no foul). Needs a match fixture; not written.
8. **`pes_*` suite**: build and run all 98 targets (the set is in `tests/**/CMakeLists.txt`) on a quiet machine. Not run this pass.
