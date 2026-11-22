# pes-port-StallLive5: the live MA locator is not established; the kick release is reverted; gate numbers on 93ad88d1 (StallLive, 20-11-26)

Status: BLOCKED on the live MA locator. Branch `StallLive` at **93ad88d1bfc404a10079aab01e4f3fd8686bec6f** (ls-remote confirmed). Parents: 303dca0b (layer + the kick-release hunk, mixed) then 93ad88d1 (revert of the hunk only). The PES-measured questions (1) are NOT answered. No behaviour change landed this round. The build dir is deleted.

## 1. Live questions: not answered (CONFIRMED that the locator fails)
Asked: during a pass flight, does MA+0x2601C keep the passer, go to 0xFF, or change at the receiver's touch; and what holds MA+0x2601C while T+0x78 is set.

Live reads (pread of /proc/<pid>/mem, sudo -n; PES pid 2193388 then 3692640; no writes, no signals):
- Reads that work: G = *(0x1436F3DC8); MI = *(G+0x270); MI+0x1198 frame; MI+0x116C play state (1 in play, 2/3 stoppage). Team records T_s = *(G+0x1928+0x28 s) with pass target T+0x78 (values 255 none, 5, 21: plausible).
- Entry chain (Main's): P = *(G+0x19C8+0x28 i) is the 0x3F40 block (qword 0 of the 0x28 entry). *(P+0x3AC0) is float-like garbage for all 22 players (diag_direct.py). The alternative PM+0x7E0 form gives 0x100000000 (not an MA). The team-AI form *(T+0x508) gives nothing plausible. A consistency scan of every anime-block word under three forms (must be reached by >=3 players of one side) gives 0 MAs (diag_scan.py).
- W1 hook (holder_w1_140457070, Main's; captures live2/pid3692640 seq032..seq085, 8 read): rcx = 0x28a57000 in every capture (team AI, one side). *(0x28a57000+0x508) is 0 in live reads (changed once, then 0). The capture's rsi = 0x7ff5ff5a0018 holds a plausible record (ball on pitch, carrier/owner 255 most of the time).
- Series from 0x7ff5ff5a0018 (150 s, 799 frames, ma_pass.csv): carrier toggles 255->4->255 (22 frames) and 255->8->255 (63 frames) with NO pass target set (T+0x78 = 255 at both toggles), ball speed jumps of 600-4300 m/s between consecutive frames. Not coherent with the live ball, so it is not treated as MA.
- Conclusion: the MA locator is not established. The pass answers need a validated MA (or a hook that reads MA directly with a verified field). INF: the W1 hook's rsi is a register copy of a different record.

## 2. Branch and commits (CONFIRMED)
- 303dca0b: "PES layer: direct callees of the trap/finish closures" — its contents also include the kick-release hunk (match.hpp). That mismatch is a hygiene error I made; the commit is pushed and not rewritten (owner rule).
- 93ad88d1: reverts exactly the 5-line kick-release hunk (`git diff` against 303dca0b: 5 deletions in match.hpp only). Subject: "Revert the kick release in Match::RecordPassEvent (no PES evidence yet)".

## 3. Gate measurements on 93ad88d1 (CONFIRMED runs)
- `gen_translated.py --check`: CHECK_RC=0, 10,150 functions byte-identical (3,824 recorded, 6,326 closure).
- Build: ok (pes_restart_delay_test, pes_throwin_release_test, pes_pass_target_test, pes_hang_test, gfsim_run).
- `PesStall.*` (r5_stall.log): 6 PASSED, FAILED `ALiveBallNeverLiesStillLongEnoughForTheBreaker` (36 firings, was 606 at base f2458223), PASSED `TheWorldHolderFieldIsThePesHolderEveryTick`, PASSED `ATrapTouchIsTheBodyScansGrantAndNothingElse` (StallContact's). `ATrapClipIsNever...` 672 s OK.
- `pes_pass_target_test`: FAILED `EveryPassInFlightHasItsOwnSidesReceiver` (receiver null: the kick release that made it pass is reverted). The stale test stays the skipped named gap.
- `PesHang` (`SeedElevenTwoMinutePesLoopMatchReachesFullTime`): PASSED (646,843 ms).
- `pes_throwin_release_test`: 6 of 9 completed before the 3,000 s sim cap (killed, no RC line). Ankle test FAILED: **2 of 109 taking ticks** kept the nearer ankle short of F[1]+0.1 m, worst **0.0155 m** (previously 2/109 on 7534f5b4 too). Not chased (seed lists are Main's).
- Smoke, seed 7, 2 min, `gfsim_run --pes-loop`: RC=0, **37,524 sim ticks to full time** (cap 194,400). This is the normal range; the 136,465 of 303dca0b was the stall with the kick release.

## 4. What the numbers say about the stall (CONFIRMED on 93ad88d1)
- The live-ball breaker fires 36 times (2 HighPass carrier-brain episodes, non-holder 0.8-1.2 m from the ball, PES holder far away). That is the remaining stall, and it is the hasIt band (see StallLive3/4).
- The kick release (holder cleared on every kick) cleared the pass receiver (a PES fact the pass test encodes) but produced 662 holderless firings. So the loose-ball approach for a holder-less ball is the real gap for that route, which is a separate port. PES's behaviour on a kick is NOT measured here, so no PES-faithful answer is available yet.

## 5. Open items
1. Validated live MA: a hook that reads a verified field of MA (e.g. on W1 at entry, read MA via the function's own argument chain), or an MA-field write watch on MA+0x2601C during a pass.
2. The pass questions (passer kept / 0xFF / changes at the receiver's touch; what holds MA+0x2601C while T+0x78 is set): blocked on 1.
3. Hasty clearance: the kick release is NOT in the branch; any kick-holder behaviour needs the measurement from 1 first.
4. Hygiene: 303dca0b's subject does not describe its match.hpp hunk; history is not rewritten (owner rule). 93ad88d1 reverts that hunk.

## 6. Scratch
Scripts in `<workdrive>/gf-StallLive-scratch/`: live_sampler3.py, diag_direct.py, diag_forms.py, diag_scan.py, w1_series.py, ma_series.py, ma_pass.py; series ma_pass.csv (150 s). Unpacked copies: none made. Build dir `<workdrive>/gf-StallLive/build` deleted.
