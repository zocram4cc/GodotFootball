# PassLane2: the PASS_GET aim is the last entry of the man's position ring (0x140A76240), which GF does not refresh; the per-frame cadence is PES's but is not sufficient; nothing landed

Branch `PassLane` (origin tip `7104ed38d`, ls-remote confirmed, unchanged by this lane). Worktree `<workdrive>/gf-PassLane` is clean (probes and port reverted, symlink restored, build dir deleted). Scratch `<workdrive>/gf-PassLane-scratch`: probe patches, logs, decompiles, helper scripts. CONFIRMED = read this session (decode, asm, capture, or test run). INF = inference. OPEN = blocker named.

## 0. Hook requests (for Main; none armed by me)
Hook VAs go through `untranslated_callees.py hooks_for_image` before arming (brief 17-11-26). Proposed, not yet run through it:
- **H1 (aim writer):** entry of `FUN_140514940` (`0x140514940`), the call that writes the PASS_GET aim from the man's ring. EVERY 20, cap 24, FULL, no predicate. It shows the flag this+0x20 and the ring entry the aim is copied from.
- **H2 (ring accessor):** entry of `FUN_140A76240` (`0x140A76240`, the ring accessor: count byte P+4, entries P+8..). It is a per-man function, so the brief's rule applies: EVERY 1000, cap 24, FULL, predicate `b(rcx+4)>0`. It shows whether the ring changes per frame for a moving man.
- **H3 (already on disk):** `0x14094E0E0` (ActionPassGet tick) has 24 real captures (`pid480109` seq001-027, `passget_tick_14094E0E0`); `0x1405141F0` (finalize) has 14 mixed-action captures (`pid1858437`). No new hook is needed for the tick or the finalize.
- Ring writer: **not found**. H1 and H2 are needed to see whether the ring is written per frame in PES, and by which caller. The caller is the next hook after that.

## 1. Chain from the test to the PES writer (CONFIRMED unless marked)
1. The test reads R+0x894 (`kRTargetOffset`), the aim. GF's `_PassGetApproach` uses it as `desiredTarget` (`elizacontroller.cpp:1231-1248`).
2. R+0x894 is written by the finalize `0x1405141F0` from its local `uStack_90` (decompile `dec_pg.txt`, the copy `if (param_3+0x894 != &uStack_90)`).
3. `uStack_90` is set by a chain of vtable slots of the ActionPassGet object (vtable `0x1426301E0`, flat-image reads, `dec_slots.txt`):
   - +0x10 tick `0x14094E0E0`; +0x80 `0x140519550`; +0x88 `0x14094E680` (writes a speed float, not a point); +0x58 `0x141863350` (returns 0); +0x98 `0x14094E3A0` (pitch clamp of a point); +0xA0 `0x14094E380` (copies this+0x4C to R+0x888, the look point); +0xA8 `0x14094E530` (copies this+0x58 to R+0x8A0).
4. **The action word is 5 (PASS_GET) in every real call.** `*r9` at the tick is 5 in all 24 `passget_tick` captures (`pg_scan_all.txt`, CONFIRMED by reading `mem.bin` through `call.bin` r9).
5. **For action 5, the selector `0x140519550` writes nothing.** Its `switch(*param_4)` jump table, decoded from the image (VA `0x14051A424` byte table, `0x14051A414` dword table, base `0x140000000`): value 5 maps to `0x14051A40B`, the default early return. The writers are the arms for 2, 3, 7, 9, 10, 0x33, 0x54, 0x6C, 0x34, 0x56 (CONFIRMED by the table, not by Ghidra's labels).
6. **So the PASS_GET aim comes from `FUN_140514940`** (`0x140514940`, called in the finalize with `param_4 = &uStack_90`). When its flag `this+0x20` is set, it does `*param_4 = FUN_140A76240(man)` (`dec_514940.txt`, CONFIRMED).
7. **`FUN_140A76240` returns a point of the man's ring:** `if man[4]==0 return man+8; else return man+8+12*man[4]` (`dec_a76240.txt`, CONFIRMED by the bytes at `0x140A76240`: `movzx eax,[rcx+4]; test al,al; ...; lea`). Ledger notes call it "history position" (`pes-re-lane-units2.md:290`, INF on meaning).
8. **The stuck aim in GF is a stored ring point, not the man and not the ball (CONFIRMED, probe5).** Slot 20, side 1: aim (-3.57, 32.83) for 40+ consecutive ticks while the man walks (23.3, -5.5) to (25.0, -6.0) and the ball is (27.9, -7.7). Slot 9, side 0: aim (-1.77, 14.35) fixed for 60 ticks while the man walks (18.9, 9.2) to (18.5, 9.3) and the ball is (22.3, 7.0).
9. **The same chain tracks the ball when it is refreshed (CONFIRMED, probe4, per-frame port).** Slot 20: aim (-25.36, -17.64) to (-25.42, -17.81) while the ball moves (-25.33, -17.52) to (-25.41, -17.77). The man walks from (-20.9, -8.7) to (-21.6, -11.0), toward the ball.
10. **Consequence:** PES's aim is the ring's last point, and the ring is refreshed each frame (PES) but not in GF. Which writer refreshes it is OPEN (section 3).

## 2. Cadence (CONFIRMED in GF, INF on PES's rate for this exact tick)
- GF calls `RunPassGetAction` only from `_PassGetApproach`, which runs inside the humanoid's requeue (`humanoid.cpp:505-525`, `RequeueOnFrame`), so the tick is not per frame.
- PES runs ActionPassGet every AI frame for a PASS_GET man (`pes-port-PassReceiver2.md` note: "~54 calls/s for 0.02-2.94 s a pass"; the hook for it was EVERY 40 for that reason, so the rate is INF for every frame).
- Per-frame port (team tick, `Team::RunPesPassTarget`, plus the controller reading the record): probe4 shows the stuck case tracking the ball (section 1.9). Probe5 (the same port, rows with aim more than 10 m from the ball) still shows frozen far aims in other episodes, so the cadence is necessary but not sufficient (INF that it is necessary).
- Port tests (CONFIRMED, `port_cadence_REFUTED.patch`, `port_and_probe_final.patch`): `ThePassGetAimIsTheBall...` 51.32 m (was 43.26 m); `EveryPassGetEpisode...` 10 of 30 over 2.94 s (unchanged); `ATrapsRootMoves...` red (pre-existing); `EveryPassInFlight...` green. Port **not landed**.

## 3. What is left unknown: the ring writer (OPEN)
- Scans run this session over every `.pdata` function (`tools/scan_ring.py`, `tools/scan_ring2.py`):
  - functions that load a man position (+0x55C/+0x564) and store a byte at +4 and a float at +8/+C: `0x14056CB00`, `0x140927D50`, `0x140A6C550`, `0x140A76589`.
  - functions that call the man accessor `0x140A48A10` and store a byte at +4: `0x140473990`, `0x14056CB00`, `0x140927D50`, `0x140A1CC64`.
- Decoded so far: `0x140A6C550` is the man-record reset (zeroes +0x55C, +0x564, ...), not a ring writer. `0x140A76589` writes a 12-byte record with its count at +2 (a touch/receive record), not the man's ring at +4. `0x14056CB00`, `0x140927D50`, `0x140473990`, `0x140A1CC64` are large; not reduced (OPEN).
- The ring entries that GF has are stale because nothing in GF writes them (INF; the GF man record's +4 and +8.. are not in `pesworldrecord.hpp`'s man layout, which has no ring field: `kManPosXOffset` 0x55C and the rest, `pesworldrecord.hpp:224-241`). Not verified by a probe of the ring contents.
- An existing ponytail names this as a stand-in: `pescarrier.cpp:2181` ("the history position (0x140A76240, S17) stands in with the current one"). That stand-in is in `pescarrier.cpp`, not on the PASS_GET path; the PASS_GET path reads the ring directly, so the stand-in does not cover it.

## 4. Task 1 and the team.cpp:445 comment
- PES's ball samples (B+0x88+0x28j) are written by GF's `Ball::CalculatePrediction`: all 250 samples lie within 0.25 m of the ball at every probed tick (CONFIRMED, probe). The aim is not read from zeroed samples. The PES sample writer (`0x140A6CF70` is only the address calculator) stays undecoded for this symptom.
- `team.cpp:445` ("the prediction samples 1..249 stay zero") is false (CONFIRMED). Not changed: no port landed, and Main asked for the fix in the port's commit. It stays for the landing commit.

## 5. Measurements (all runs on this lane's tip plus the probes)
| run | result |
|---|---|
| baseline `PesPassTarget.ThePassGetAim...` | RED 43.26 m (PassLane1), again 43.26 m |
| port (per-frame cadence) `ThePassGetAim...` | RED 51.32 m |
| port `EveryPassGetEpisodeIsWithinPesLiveRange` | RED 10 of 30 over 2.94 s (same as baseline) |
| port `ATrapsRootMovesByExactlyTheStrikeAdjusterStep` | RED (pre-existing) |
| port `EveryPassInFlightHasItsOwnSidesReceiver` | GREEN |
| `IsSetInFlightAndNeverStaleWhileTheBallRests` | SKIPPED (named gap) |
| PesStallQuick, ALiveBall (ALIVEBALL line), census delta, PesHang | NOT run: no landed behaviour change. Main's ALiveBall: not requested (nothing to measure). |
Test binary: `build/tests/sim/pes_pass_target_test` (built with ccache; one build per probe). Seed 7, 2 min, pes_loop. No mutation checks (no landed change).

## 6. Recommendation (not landed)
- **Land nothing from this lane yet.** The cadence port alone moves the max aim from 43 to 51 m. Land it only with the ring writer, so the aim is refreshed per frame from PES's ring, and then the test can go green.
- **Next, in order:** (1) Main arms H1 and H2 (section 0) through `hooks_for_image`. (2) From H2's per-frame ring contents, find the writer: the ring entries that change per frame (for a moving man) name the writer's inputs; decode that writer statically (candidates in section 3 are unverified). (3) Port the writer and the per-frame cadence together, red-first on the aim test; fix `team.cpp:445` in that commit; then measure PesPassTarget, PesStallQuick, ALiveBall and the census.
- **Task 3 (TRAP reach gate for receivers at 0.06-0.28 m):** not started.

## Artifacts (scratch, not committed)
`probe_aim.patch`, `port_cadence_REFUTED.patch` (per-frame cadence port, 4 files), `port_and_probe_final.patch` (port plus probes), `probe1.log` (43.26 m), `probe3.log` (man vs aim: stuck), `probe4.log` (per-frame: ball tracking), `probe5.log` (per-frame, rows >10 m: stuck far aims), `pg_scan_all.txt` (action word 5 in 24 captures), `pw_scan.txt` (finalize captures, mixed actions), `dec_pg.txt` (0x14094E0E0, 0x1405141F0, 0x140945EC0), `dec_slots.txt` (vtable slots), `slot80.c` (0x140519550), `dec_514940.txt`, `dec_a76240.txt`, `dec_ring.txt`, `dec_ring2.txt`, `scan_ring.out`, `scan_ring2.out`, `tools/*.py` (capmem, pdata, callers, pg_scan, pw_scan, scan_*).
