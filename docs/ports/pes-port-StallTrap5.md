# pes-port-StallTrap5: counter-B writer 0x14092ED30 translated, proven on 12 real captures, layer --check green, pushed (aaac3218); feed NOT landed: the writer returns 0 for a standing man in GF's window (StallTrap, 22-11-26)

Status: PARTIAL. Landed: translation (aaac3218, on origin/StallTrap, confirmed by ls-remote). Not landed: the feed switch in Team::RunPesPassTarget and the red race test. Reason: with the translated writer in the feed, counter B is 0 for every man in GF's world, so the feed would zero the race's input. Work reverted, WIP saved (section 6).

## 1. Pushed: `aaac3218` (CONFIRMED)

- Closure from 0x14092ED30: 154 members, 136 entered by the 12 real replays. Protector and dispatch addresses 0x1415A1720, 0x1415A174A and 0x141794410 are not in the proven set.
- Proof: `prove_bounded.py 0x14092ED30 --real x12 --merge` (captures live2/pid678718 seq392, 394-397, 399, 401, 402, 404, 407-409): 12 calls, 12 replayed, 0 mismatches, 0 excluded, own-block 61/106 (0.5755). Record in `results_trapaction.json`; closure in `closure_counterb.json`.
- `gen_translated.py --image pes_full.bin --check`: 10211 functions regenerate byte-identical (rc 0). Before that: regen wrote the layer (rc 0).
- Commit content: the layer (`src/pes/translated`), 7 `ro_tables.json` windows, `UNHANDLED_DEAD_NOISE` entry for 0x140706350, universe rows, 105 trapaction specs, `closure-counterb.txt`.

## 2. ro_tables: each bound from the code (CONFIRMED unless marked)

| table | read site(s) | index bound (from code) | window |
|---|---|---|---|
| 0x1425DDCD8 | 0x1406B8DF2 | edx 1..5 (`lea -1(%rdx),%eax; cmp $4; ja` at 0x1406B8DE0..DE6) | 0x18 |
| 0x1425DDE10 | 0x1406B9E6F | edx 1..3 (`cmp $3; ja`, `test edx; jne` at 0x1406B9E50..E57) | 0x10 |
| 0x1425DDE2C | 0x1406B9AC3, AEC, AF5, AFE | loop pointer E38,E34,E30; ecx 0..2 at the table reads | 0x40 |
| 0x1425DDF10 | 0x1406B9B70 (and the `cmp` at 0x1406B9B8E) | ecx 1..10 (`cmp $0xa,%eax; jb` loop); ecx 1..9 for the compare | 0x28 |
| 0x1425E9F10 | 0x14078E50D | edi 0..2 (set by the `ja`/`setbe`+inc at 0x14078E4D3..E4E4) | 0x6 |
| 0x1425DDA50 | 0x1406B9C02 | INF: index = a + 8*b from caller memory; extent to the next table start 0x1425DDCD8 | 0x288 |
| 0x1425DD988 | 0x1406B9C0D | INF: index from caller memory; extent to the next table start 0x1425DDA50 | 0xC8 |

The two INF rows have no guard in their function; the bound is the caller's and is not checked. Reads past the window return 0 silently.

## 3. Held out and dead noise

- 0x140706350: dead-noise entry (OUTSB/LOOP/FIMUL/FILD/OUT at generated lines 2441-2521, after the unconditional goto at 2422; every goto into the block stays inside it). Kept with its evidence.
- Held out (unchanged): 0x1403BB670, 0x1405549C0, 0x14202BA30. None is in the root's closure, so the root is not held and its proven path needs none of them.

## 4. OPEN, found while doing section 2: unlowered image reads in 0x1406B9B20 (must fix before the layer is used)

- The loop `0x1406B9B20` picks its base pointer with `cmove` (0x1406B9B53: r11 = 0x1425DDEB0 or 0x1425DDEE0). Its reads at 0x1406B9B97 (`addss (%rdx),%xmm1`) and 0x1406B9BA2 (`addss (%r11,%rax,4),%xmm0`) read image .rdata through a register whose origin the lint does not track. The lint reported only the 0x1425DDF10 reads, so these two are NOT declared, not baked, and not flagged.
- In the GF runtime (no image mapping) these reads return 0. The harness maps the image, so the 12 proofs stay bit-exact there. Fix: teach `image_origins` the `cmove` (both candidates are image VAs), then declare 0x1425DDEB0 and 0x1425DDEE0 with a 0x28-byte window each. Not done.
- Same class of gap for any other `cmov`/`cmove` base in the closure. Not audited.

## 5. Feed and red test: NOT landed (CONFIRMED diagnosis, the blocker is still open)

What was built and then reverted (patch at `counterb-feed-wip.patch`, 204 lines):
- `PesWorld::RunCounterB(slot, playState, writeLog)`: calls `::pes_fn_14092ED30` with rcx = ManVa(slot), rdx = SlotVa(slot), setting and restoring MI+0x116C like RunPassTarget.
- `BindSlot`: man+0x7E0 = CtxVa(side) (`kManCtxOffset`). Evidence: the proof spec's regions `A0.7E0` (`+0x18` and `+0xE0` subregions) match the side context `Ctx` (kCtxMaOffset 0x18, kCtxEventsOffset 0xE0). Without it the writer reads 0 from man+0x7E0 and exits.
- `Team::RunPesPassTarget`: the counter-B store (SR+0x1438, tt*54/1000) replaced by `RunCounterB` per on-pitch slot after the ball is staged; the stand-in ponytail removed.
- Red test `PesCounterB.ANearRestingBallsManHasTheSmallestCounterBAndLeadsTheArrivalList` (ball 1.2 m from place 10, others 1.8 m+, other side 50 m away; asserts the nearest man has the smallest counter B and leads the arrival list). Red stage 1: compile error, `PesWorld` has no member `RunCounterB` (CONFIRMED). Stage 2 (with the API): counter B 0 for all men.

Gates found by probing (CONFIRMED, probe output):
1. The writer exits at `0x14092EDC5` if `AI+0x8D` is set, and at `0x14092EF72` unless `sil` is set. `sil` needs `r12d` (frames since the newest event, `0x140A51320`) == 0, or == 2 with a team flag clear. Event at this frame: `framesSince=0`, the writer runs (5041 to 8194 writes).
2. After the context binding: the writer runs to completion but counter B stays 0 (`no-samples=0`, `with-samples=0`), even with ball prediction samples 1..16 set to the ball's position.
3. So the value that reaches SR+0x1438 (eax from `0x140928120` -> `0x678950` -> `0x6788B0`/`0x678990`) is 0 for a standing man 1.2 m from a resting ball in GF's window. The inputs that differ from the captured states are not yet identified.

Next step (exact): decode the return path of `0x678950` and the record fields `0x6788B0` / `0x678990` read (man velocity or reserve fields, ball prediction samples 1..249 with their stride, MA ball list), then compare those values with the proof's real call of pid678718 seq392 (the spec's regions, 0x14092ED30.json). Whichever field is zero in GF and nonzero in the capture is the next port. Until counter B is nonzero for a standing man, the feed must not land.

## 6. Artifacts and state

- Pushed: `aaac3218` on origin/StallTrap (gen --check green at that commit). Branch tip matches origin.
- Worktree clean at `aaac3218`. `build/` deleted. Scratch: `counterb-feed-wip.patch` (feed, RunCounterB, BindSlot context, red test, temp probe), `counterb-tracked.patch`, `counterb-untracked.tar`, `closure_counterb.log`, `prove2` deleted, logs `gen2.log(.check)`, `t1..t7.log`.
- Not run: PesStallQuick, full ALiveBall, PesHang, PesPassTarget (no feed to measure). The test binary `pes_events_test` ran for the new tests only: `PesFramesSince`, `PesArrivalList` green; `PesCounterB` red (the WIP; not committed).
- No captures remain unpacked in scratch.

## 7. Confidence

- Writer bit-exact on the 12 real calls: CONFIRMED (snapdiff).
- Layer equals the proof and regenerates byte-identical: CONFIRMED (`--check`).
- ro_tables windows (section 2): CONFIRMED for the five code-bounded rows, INF for the two caller-indexed rows.
- Cmove blind spot (section 4): CONFIRMED by disassembly and by the lint's silence; the GF-runtime effect is INF until a run.
- Writer returns 0 in the GF window (section 5): CONFIRMED by probe; the cause is open.
