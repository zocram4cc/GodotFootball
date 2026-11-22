# pes-port-StallTrap7: counter-B read-set diff; the pushed CMOV origin (5b67bc56) aborts every --pes-loop match at boot; counter B still not fed (StallTrap, 22-11-26)

Status: PARTIAL, BLOCKED. Nothing landed, nothing pushed. origin/StallTrap = 5b67bc56 (ls-remote confirmed); worktree restored to it; build/ removed.
Acceptance not met: counter B is not nonzero in GF, no feed, no race test. The read-set diff table and the first unbound field are delivered (section 3).

## 1. BLOCKER on the pushed tip (CONFIRMED, gdb backtrace)

`gfsim_run ... --pes-loop` on the translated layer from 5b67bc56 aborts in the Match constructor, before any tick:

    PES_RO_OUT_OF_WINDOW: table at ... (1 bytes): read of 1 bytes at offset 200762705678
    #3 ro_rd (t=k_t142583E32_1, len=1) ... #5 pes_fn_1403B8170 ... #6 pes_fn_1403B8090 ... #13 pes_fn_141EA7640
    #17 PesFoxWorld::PumpUntilIdle #18 PesFoxWorld::LoadBody #19 AnimeWorldTemplate #21 Match::BindPesTrapWorld (matchevents.cpp:93) #23 Match::Match (match.cpp:116)

The table `0x142583E32` (1 byte, the "empty-string fallback" the CMOV fix declared) is read by 0x1403B8170 through a `cmove` base whose other candidate is a runtime pointer. The CMOV-origin fix routes that base through `ro_rd` (window-checked); before it, the same read went to memory and returned 0. A runtime pointer is now an out-of-window abort. The same mechanism covers the 0x1406B9B20 base of section 2.

Recommendation (not done here): revert 5b67bc56 or fix the translator so a `cmove` base whose candidates include a runtime value reads through `s.mem.rd`, not `ro_rd`. Until then no --pes-loop sim runs on this tip.

## 2. Read-set diff (CONFIRMED where marked)

PES side, seq392 (pid678718, call entry; capture unpacked to scratch, then deleted):
- Entry regs: rcx = man 0x1d7a0c80, rdx = slot 0x1faf7958, rsp 0x1c4ff358.
- Unicorn ordered trace of the same call (rs/ordtrace.py, PES's own image code over its memory): 27,145 accesses (17,170 reads); 15,217 reads inside man/slot/ctx/MA/EV/obj.
- Writer's own counter-B store: `W pc=0x14092F204 addr=0x1FAF8D90 (SR+0x1438) value=0x2EC` = 748 frames. CONFIRMED (trace).

PES values at the writer's man/slot reads (readset392 + capture bytes; rs/pesreads.txt):

| read (base, off) | size | PES value | meaning (INF unless said) |
|---|---|---|---|
| man+0x18 | 4 | 0x12 (18) | player/slot index; writer forwards it to callees (INF) |
| man+0x624 / +0x62C | 4 | 0x40791B5C = 3.8922949 / 0x412046B8 = 10.0172653 | the man's x and z; the writer reads these, not +0x55C (CONFIRMED by readset) |
| man+0x698 | 4 | 1 | |
| man+0x7D8 | 8 | 0x347EC830 (pointer to a position object: x at +8) | |
| man+0x7E0 | 8 | 0x1D688140 (side ctx X) | |
| man+0xAD28 | 8 | 0x347EC830 | |
| slot-0x1E28 / -0x1E20 | 4 | = man+0x624 / +0x62C values | |
| slot+0x1430 | 4 | 0xF9 (249) | counter A |
| ctx+0x18 | 8 | 0x7FF5FDC70018 (MA) | |
| ctx+0xE0 | 8 | 0x7FF5FDAE0018 (EV, events store) | |

GF side: the unit world (BindSlot, positions at +0x55C/+0x564, no ctx binding) reads 0 at 39 of the 45 man/slot offsets that PES reads non-zero (rs/offs.inc, rs/offs_meta.json; probe output in scratch). Without the ctx pointer the writer returns at once (man+0x7E0 = 0).

Ordered comparison (rs/compare_trace.py): GF probe with the ctx bound (man+0x7E0 = CtxVa(side)), ball sample 0, a kind-7 event at this frame, MI play state 1:
- #0..#2 man+0x7E0, ctx+0xE0, ctx+0x18: equal (canonical object offsets) once ctx is bound.
- #3 man+0x18: PES 18, GF 0. FIRST VALUE DIVERGENCE on the path (the writer forwards it to its callees at 0x140A62690 and stores it into slot+0x5140..0x5148).
- #6 EV+0x3208 (ring-1 tail, 12808): PES 2, GF 0. FIRST CONTROL DIVERGENCE: the path then differs (PES reads the 0x140A3E190 event search; GF exits). Cause here is the event history, not a man field: PES's ring holds earlier events; the probe routes one kick. In-match this is GF's own event flow, which I could not run (section 1).

## 3. First unbound field and its writer

- Unbound on the value path: man+0x18 (player/slot index; GF 0). PES writer: NOT FOUND. The record docs (pes-re-player-record.md) name no writer for it. OPEN: needs a static scan for stores to [man+0x18] in the translated closure (or a PES capture pre/post pair around the man's creation).
- Bound and confirmed necessary: man+0x7E0 = side ctx X (PES's ctx value 0x1D688140 in seq392). Its writer is the side's AI context setup in BindSlot (the ctx pointer is stored as CtxVa(side), as the WIP did). Reads show man+0x7E0 is the first read of the writer; without it the writer returns at once.
- Event ring (EV+0x3208 etc.): state, not a binding; comes from GF's own Route calls in a live match. Unverified.

## 4. Counter-B feed: not landed

Diagnostics only (saved as scratch/counterb-diag7.patch, 266 lines; reverted from the worktree):
- PesWorld::RunCounterB (rcx = man, rdx = slot, rsi = man), BindSlot ctx binding (kManCtxOffset 0x7E0), feed in Team::RunPesPassTarget before RunArrivalList (the store at team.cpp 428-430 removed), GF_CB_DUMP ordered-read dump, and a temporary read hook in PesMem::rd (src/pes/translated/pes_state.hpp: a file-backed trace). The hook forces a full translated rebuild; revert before any landing.
- Not run: PesStallQuick, full ALiveBall, PesHang, PesPassTarget, red race test, gen --check, push.

## 5. Next steps (exact)

1. Revert 5b67bc56 (or fix cmove base routing) and re-check --pes-loop boots. Main's call.
2. Find the writer of man+0x18: scan translated closure for stores with displacement 0x18 on the man register (pes-image-field-scan skill), then bind it from GF's slot index with the writer cited.
3. Re-run the in-match dump (patch in scratch) once (1) lands; compare with rs/compare_trace.py (GF_PATH) against PES's ordered trace.

## 6. Files (scratch, <workdrive>/gf-StallTrap-scratch/rs/)

pesreads.txt (PES labelled reads with values), offs.inc / offs_meta.json (45 offsets), pes_ord.tsv (PES ordered trace), ordtrace.py, label_pes.py, compare_trace.py, gf_ord.tsv (probe trace), counterb-diag7.patch (in scratch root). Captures: seq392 copy and unpacked mem.bin deleted; live2 untouched.

## 7. Confidence

- PES counter-B store value 748 at 0x14092F204: CONFIRMED (ordered trace).
- man+0x624/+0x62C is what the writer reads (not +0x55C): CONFIRMED (readset); the earlier "+0x55C is the writer's position" claim is not supported.
- man+0x18 unbound, value 18 vs 0: CONFIRMED (ordered compare); its meaning (player/slot index): INF; PES writer: OPEN.
- 5b67bc56 aborts --pes-loop boot: CONFIRMED (gdb).
- EV+0x3208 divergence cause (event history): INF (in-match not run).
