# pes-port-StallTrap6: CMOV origin fix landed (5b67bc56, gen --check green); counter B is still 0 in GF's world: the writer reads more PES state than GF binds; feed, red race test and sims NOT run (StallTrap, 22-11-26)

Status: PARTIAL. Pushed: `5b67bc56` on origin/StallTrap (ls-remote confirmed). The feed switch, the red race test, PesStallQuick, full ALiveBall, PesHang and PesPassTarget are not landed: the writer returns 0 for a standing man in GF's world, so the feed would zero counter B. Build dir deleted; scratch 21 MB.

## 1. Landed: `cmove` origins (CONFIRMED)

- `image_origins` (tools/dev/translate/xlate.py) skipped CMOV definitions, so a base chosen by `cmove` had no origin: its reads were neither baked nor flagged, and GF's runtime would read 0. Added `CMOV`: both the destination's prior value and the source are origins.
- Consequence (lint, CONFIRMED): 9 live reads appeared, in 0x1406B9B20 (cmove loop base) and two CRT string functions (0x140397150, 0x1403B8170). Declared in ro_tables:
  - 0x1425DDEB0, 0x88 bytes (0x1406B9B20: the two bases EB0/EE0 and the F10 walk; one window must hold all three starts).
  - 0x14257D1E0, 0xE bytes (0x140397150: the "true"/"false" literals, NUL-terminated loop).
  - 0x142583E32, 1 byte (0x1403B8170: the empty-string fallback, its byte is NUL).
- `gen_translated.py --check`: 10211 functions regenerate byte-identical (rc 0). Three layer functions changed (0x140397150, 0x1403B8170, 0x1406B9B20).
- Not run: the full pes test suite. Only `pes_events_test` was built; its PesFramesSince and PesArrivalList tests pass.

## 2. Is 0x1406B9B20 on the counter-B value path? YES (CONFIRMED)

- The 12 real replays of 0x14092ED30 entered it. There is no direct caller in the image; it is reached through a function-pointer table at 0x1425E35F8..0x1425E3688 (entries 0x1406B9AA0, 0x1406B9B20, 0x1406B9BD0, 0x1406B8DE0, 0x1406B9E50 sit in it), so the static closure walk does not show the edge.
- Before this fix its cmove reads returned 0 in GF. Fixing that did NOT make counter B nonzero (section 4), so it is one of several causes, not the only one.

## 3. Writer's real inputs (CONFIRMED against seq392)

- Caller at 0x1409294EF: `mov %rsi,%rcx` (rcx = rsi), `mov %r15,%rdx` (rdx = slot record). So rsi = the man, not a ball. My earlier `rsi = BallVa()` was wrong; the correct value is `rsi = ManVa(slot)`, which I used.
- readset (tools/dev/translate/readset.py on seq392, output in scratch `rs/readset392.json`): heap reads are rcx-based (+0x18, +0x624, +0x62C, +0x698, +0x7D8, +0x7E0 (a ctx pointer, 0x1d688140 in the capture), +0xACC8, +0xACE0, +0xAD28), rdx-based (-0x26AF..+0x1434, the slot record and an object 0x26AF below it), and the writer's stack. The writer never reads its entry r9; the r9 cluster is a callee's.
- The search's position: 0x140928120 gets `r9 = man+0xC8` and reads its position at `r9+0x55C`/`+0x564`, i.e. man+0x624 (x) and man+0x62C (z). In seq392, PES's man+0x624 = 3.8923 and +0x62C = 10.0173; PES's man+0x55C and +0x564 are 0.0. GF writes its position at +0x55C/+0x564 (kManPosX/Z), which the arrival writer reads (30/23 references) and which is proven bit-exact on 12 captures. So in this capture the two copies disagree, and I have not established which one is current.
- Layer writers: none direct. A linear sweep of the whole code section for a store to `[reg+0x624]`/`[reg+0x62C]` found none (scan624.py). The field is probably filled by a bulk copy (the caller's 0x80-byte copy loop at 0x140929400 is one such). Not traced.
- PES values at the other reads (seq392, pesvals.py): man+0x698 = 1; man+0x7E0 = 0x1d688140; man+0x7D8 = 0x347ec830; SR-0x1E28 and SR-0x1E20 mirror man+0x624 and +0x62C (3.8923, 10.0173); SR+0x1430 = 0xF9; man+0xACC8 = 0; man+0xACE0 = 0; man+0xAD28 = pointer 0x347ec830.

## 4. GF's counter B is still 0 after the binding attempts (CONFIRMED, test output)

Test: `PesCounterB.ANearRestingBallsManHasTheSmallestCounterBAndLeadsTheArrivalList` (WIP, not landed). Results, each run in the same unit world:
- rsi = BallVa: 0 for all men.
- rsi = ManVa: 0 for all men (the correct value; writer exits with the same zeros).
- + a fresh kick event at the current frame (PES's gate: framesSince = 0, writer runs, 8194 writes): 0.
- + man+0x7E0 = CtxVa(side) (proof spec region A0.7E0; `BindSlot` binding `kManCtxOffset`): 0.
- + ball prediction samples 1..16 = ball position: 0.
- + man+0x624 = x, man+0x62C = 0 (diagnostic): 0.

So at least one more input is zero. The next checks are the return values of the two callees on the path: `0x1406B9270` (a predicate; the writer's value is 0 when it returns 0) and `0x1406788B0` (returns 0 when its rcx is null). Those returns, and the fields they read (`+0xAB8`, `+0x3530`, `+0x34F0` of the ball/unit object, the `0x3B0` G-field, and the man's +0xC8 sub-record), are the next probe.

## 5. Next step (exact)

1. Probe `pes_fn_1406B9270` and `pes_fn_1406788B0` directly in the unit world (their return values, and the first field each dereferences that is zero).
2. For the same fields in seq392, read PES's value (`pesvals.py` already reads the capture; extend it to the addresses of the callees' loads). The first field that is zero in GF and nonzero in PES is the port.
3. Keep the rsi = ManVa fix and the CMOV fix; then the feed and the red test (WIP saved, `counterb-feed-wip2.patch`, 208 lines, also has the diagnostic position lines to remove).

## 6. Artifacts

- Pushed: `5b67bc56` (CMOV origin, ro_tables, three layer functions). Worktree clean at `5b67bc56`; `build/` deleted.
- Scratch: `counterb-feed-wip2.patch` (feed, RunCounterB with rsi = man, BindSlot ctx, red test, diagnostic lines); `rs/readset392.json` and `readset.log` (readset of seq392); `rs/pesvals.py` (PES values at the reads); `rs/scan624.py` and `scan624.out`; `callers_scan.py`, `closure136.txt`; the test logs `t1..t10.log`.
- Not run: PesStallQuick, full ALiveBall, PesHang, PesPassTarget, full pes_events_test.

## 7. Confidence

- Layer change and windows: CONFIRMED (gen --check byte-identical; lint clean).
- 0x1406B9B20 on the value path: CONFIRMED (replay entry), mechanism via the dispatch table: CONFIRMED by table bytes.
- rsi = man: CONFIRMED (caller bytes at 0x1409294E7..0x1409294EF).
- man+0x624 is the writer's position and differs from +0x55C in seq392: CONFIRMED (capture bytes). Which one GF should carry: INF (not established).
- The remaining zero: OPEN (section 4).
