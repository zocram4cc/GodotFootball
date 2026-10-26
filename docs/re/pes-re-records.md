# PES real records, frame drive, and per-frame writers (Records, 26-10-26)

Read-only RE. Image `pes_full.bin` (VA offset 0x140000000). No PES bytes quoted.
Labels: **CONFIRMED** = bytes/code read this pass (capstone over the image, snapshot bytes, or emu runs); **INF** = inference; **OPEN** = blocker named.
Snapshots (PES-derived, never committed): `<workdrive>/gf-Records-scratch/rec0` (frame 36422, per=3), `rec1` (frame 659, per=1), `rec2` (frame 1229, per=1). Pauses: 2.069 s, 3.194 s, 1.631 s; every STOP was CONTed (log `pauses.jsonl` in scratch). Harness scripts in scratch (`decode.py`, `pumpdrive2.py`, `consume.py`, `framedrive.py`, `forge13.py`, `fresh27.py`, `c1drive.py`, `bigest.py`, `fullframe.py`, `retcheck.py`, `step.py`, `pumpdrive3.py`, `drive.py`, `ringscan*.py`, `snaptop.py`, `binderhunt*.py`).
Function bounds below are from `.pdata` (CONFIRMED parse this pass).

Prior docs re-checked, corrected where noted: `docs/re/pes-re-advance.md` (Advance), `docs/re/pes-re-s4.md` (S4), `docs/re/pes-re-integration.md` (plan).

## 0. Headline verdicts

| # | Ticket item | Verdict |
|---|---|---|
| 1 | Real records + producer | **CONFIRMED** record format (pump ring + detector ring), producer chain to the analyze-listener family (§1–§2). One correction to Advance: the pump ring holds **input-sync** records (consumer-only), not the frame drivers. |
| 2 | Drive `0x140A115A0`, reach `0x140A128E4`, N frames, MI+0x1198 advance, 3x deterministic | **NOT ACHIEVED — precise missing piece named** (§5). The inc was reached only in static tracing, never fired in emu: every resting snapshot lacks the per-frame transient bindings, and every stale record fails its roster gate. Determinism of what *does* run: 3 identical runs CONFIRMED (§4). |
| 3 | Writers (S4 list, positions, ball, frame counter) with VA + call stack | **PARTIAL**: frame counter + positions + ball-gate named with VAs and stacks (§6). **S4 nearest-list writer still OPEN** (narrowed, §6.1). |
| 4 | Missing piece if no advance | **NAMED with evidence** (§5). |

## 1. Record format and producer (CONFIRMED)

### 1.1 Pump ring (input sync, consumer-only)

- Ring: 32 slots x 16 B at `container+0x40000`, `idx @+0x40204`, `count @+0x40208` (re-verified in rec0/rec1: `25/2`, `10/3`). Slots are a linked node chain (each slot's two qwords chain to neighbours), pending = `idx..idx+cnt-1 mod 32`. CONFIRMED.
- Node layout: `+0x0` w0 (`0x400002`: type 2, chan 0, stride 32 = `w>>0x11`); `+0x8` timestamp double (QPC µs; pending spaced ~17.5 ms: rec0 `141977424944.3` → `141977442488.2`); sections from `+0x20`, each `w` with `type=w&0x7f` (always 4), `chan=(w>>7)&0x3ff` (0, 2, 404–438), `stride=w>>0x11` (16/32/48/80); terminator `w==3`, redirect `w==1`. CONFIRMED by section-walk in decode.py on rec0 (2 records x 8 sections) and rec1 (3 records).
- Section key: `[sec+5]` (byte). rec0/rec1 sections carry keys 0, 5, 7. CONFIRMED.
- Dispatch (`0x141B3EDD0`, pdata `0x141B3EDD0–0x141B3EEE4`): per section, `t=w&0x7f`, `c=(w>>7)&0x3ff`: `c==0` → call `[hub+t*8+0x1020]` slot 2; `c==2` → `[hub+t*8+0x820]` slot 2; else → `[hub+t*8+0x20]` slot 2 (with `r9`/`rsp` timestamp args); then always `[hub+t*8+0x1820]` slot 2. Hub = pump (`0x141B3EA30` passes `rcx=pump`). CONFIRMED by disassembly.
- Section routers (`0x14197C8A0` chan-0 / `0x14197C8C0` chan-else / `0x14197C8E0` chan-2): `key=[sec+5]` → shared table `0x1437CB160` → entry vtable slot `+0x18`/`+0x8`/`+0x10` respectively. Real consumers run the **queue-append** (`0x14197C900`): `r8=[entry]`, capacity check `(sec+7)&~7+[r8+8] <= [r8+0x10]`, else return 0. **At rest the queues report full (`[r8+8]=0x23`, `[r8+0x10]=0`) so every section returns 0 — consumer-only.** CONFIRMED by emu (pumpdrive3.py: all key-5 sections `rip=ret rax=0`, key-0 section reaches a `WriteFile` import model = logging path, not game state).
- Queue drain (`0x14197C2A0`, pdata `0x14197C2A0–0x14197C2C0`): pops queued section pointers, calls `[entry+0x20]` per pointer. For pump keys the `+0x20` slots are the return-0 stub `0x140421F40` (key 0/1/3/4/5/7/9/11 table read this pass). Key 10 (`0x12F73B0`, vt `0x142A87258`) is the only entry with live code in all five slots. CONFIRMED.
- Live key→entry→consumer table (rec0, snapshot reads): key0 `0x12FBD20`, key1 `0x12FBD40`, key3 `0x12FBD60`, key4 `0x12FBF60`, key5 `0x12FBFC0` (vt `0x142A88628`: `+0x8=0x1419C4260`, `+0x18=0x1419C3C40`), key7 `0x12FBFE0`, key9 `0x12FC000`, key10 `0x12F73B0`, key11 `0x12FECE0`. (An earlier pass misread these from the *image* `.data` instead of the snapshot — image holds stale template addresses; snapshot holds the live ones. Corrected this pass.) CONFIRMED.
- Direct consumer drive (consume.py, rec0): all 20 sections × (chan + default) consumers run clean (`bad=None` except key-0 logging path needing an import model); **0 writes** to MI/MA/man; census 625 writes all emulator-stack/pool noise. Consumer-only CONFIRMED, deterministic over reruns (625/625 same distribution).

### 1.2 Detector ring (the frame drivers) and its producer

- Ring: slots `(idx, word)` at `E+0x26FF0+s*8` (64 deep), `count E+0x271F0`, `head E+0x271F4`. Fetch `0x140A3E570`: `idx<10` → `E+idx*0x1908+word*0x320`, else → `E+0xFA50+word*0x320`. Count fn `0x140A3E8E0` (`[rcx+0x271F0]`). CONFIRMED by disassembly.
- Producer `0x140A3DC40` (pdata `0x140A3DC40–0x140A3E13D`): `(rcx=E, rdx=record)`: type `[rdx]` → slot table `0x142642E60` (`6→10`, `7→0`, `1/2/10→10` — read this pass); copies `0x320` B into the slot ring (`0x140A3DB80` for `idx>=10`, `0x140A3DAC0` for `idx<10`); `E+0x271F0++` (clamp `0x40`), `E+0x271F4++` (mod `0x40`), slot write. Type `0x1F` special-cased to slot `0xA`. CONFIRMED by disassembly.
- 6 direct `E8` callers (byte-scan CONFIRMED): `0x140A0D4C8` + `0x140A0D5F8` (both in detector loops over `[D+0xB600]` with `0xDF` iterations, records built by `0x140440D50`+`0x140A3CDD0`), `0x140A0E0D3` (via `0x140A867D0`/`0x140A3E7D0`), `0x140A0E17F`, `0x140A0E2A6` (builds **type 7** via `0x140A3D060`: `[0]=7,[4]=r8d,[8]=frame`, `edx=7`), `0x140A0E486`. All in the analyze-listener family (`0x140A0D190`/`0x140A0D3F0`/neighbors). CONFIRMED.
- Driver `0x140A13060` (pdata `0x140A13060–0x140A13126`): `(rcx=D-holder, rdx=MI-ish struct)`: per-slot `fetch → 0x140A115A0(D, E+0x180C60, record, rsi)`, then tails `0x140A11290`/`0x140A107C0`/`0x140A12B60`. Sole `E8` caller: `0x140A0D35A` inside `0x140A0D190` (pdata `0x140A0D190–0x140A0D302`), which first runs per-record setup `0x140A22130` (pdata `0x140A22130–0x140A226C1`). CONFIRMED.
- **Switch correction** (overrides the advance doc's type→case table): the dispatch uses the *index byte* (`[0x140A12B20+([rec]-1)]`) as the jump-table index, not the type. Only idxbytes 0–10 dispatch; **idxbyte 11 (types 2,3,5,6,9,10,12 and most others) exits at once** (`offsets[11]=0x140A1223A`). Verified: `type→idxbyte→case`: 1→0→`0x140A12564`, 4→1→`0x140A12937`, 7→2→`0x140A11634`, 8→3→`0x140A119B4`, 11→4→`0x140A126CE`, 13→5→`0x140A1281A`, 23→6→`0x140A11AC3`, 24→7→`0x140A123AB`, 25→8→`0x140A11A62`, 27/28→9→`0x140A12736`, 37→10→`0x140A12A1F`. CONFIRMED by table reads + emu trace (type-9 record demonstrably lands on `0x140A1223A`).
- Stale bodies persist in the rings after drain (`Ecnt=head=0` in all 7 snapshots, but bodies churn: rec1→rec2 changed 40+ `idx10` bodies, types 6/9/10/12/15/24/25/27/38/44/45 observed). Fresh examples (rec2): type-27 `w=0x23` (`+30=0x18`, `+34=1`), type-6 `w=0x8`. No type-13 with `31a!=0` anywhere (all `31a=0`). CONFIRMED.

## 2. Where the producer is (thread/loop) — INF with CONFIRMED anchors

- The detector producer's callers are the analyze steps driven by `0x140A0D190` and its sibling `0x140A0D3F0` (c1 detector loop), themselves reached from analyze-listener step 5 `0x140426760` ← step dispatcher `0x14042B9F0`, on the same type-2 pump record as H (plan §1.2). The c1 loop's `r14` = incoming `rdx` = `[analyzeObj+0x20]`; `rbp` = D-holder. INF (chain), CONFIRMED (each callsite disassembled).
- The pump ring's producer (input sync) is a second thread: `lock xadd` on `+0x40208` + `xchg` freelist at `+0x3FFF0` in the drain paths, timestamps spaced one frame apart, `P+0x4198` cycling 2–3. INF (lock-prefixed consume + async timestamps), consistent with Advance §b.
- Which thread runs the analyze steps vs H (order OPEN in the plan) is still OPEN — no thread IDs are observable read-only. §5 names what blocks it.

## 3. Driving the processor (what ran, honestly)

- Pump dispatch with real pending records (rec0): 40 listener calls, `bad=None`, frame/MI/man unchanged, 0 game writes. Consumer-only CONFIRMED (×2 runs identical).
- `0x140A13060(D, MI)` baseline: reaches tail `0x140A11290`, faults (`movzx edx,[rax]`, `rax=0`) because `[MI+0x68]=0` in every snapshot — the tail's roster binder is a per-frame transient. CONFIRMED (regs dump).
- Direct `0x140A115A0(D, ES, record, MI)` with stale records: type-9 → switch exit (idxbyte 11); type-13 (forged `[0x24]=[8]-[4]`, `31a=1`) → reaches `0x140A1281A` body, exits at `0x140A1282C` (`0x140A0E690` returns `0xFF` > `0x15`: roster scan for `[slot+0x10]==edx(0)` finds nothing at rest); fresh rec2 type-27 → reaches `0x140A12736` body, exits on `[r8+0x30]=0x18` (`jae`). **No drive reached `0x140A128E4`; no run wrote ES/MI/MA/man.** CONFIRMED (forge13.py, fresh27.py traces).
- Pre-consume setup `0x140A22130(D, MI, forged-S13)`: `bad=None`, 149 writes into `E+0xCE964` scratch, none to counters/MI/MA/man. CONFIRMED (fullframe.py).
- c1 produce loop `0x140A0D3F0(D, MI)`: 223/223 detector slots null (`test rbx,rbx → je`), `Ecnt` stays 0. CONFIRMED (c1drive.py) — `[D+0xB600]` detectors are unbound at rest.
- Whole `0x140A0D190(D, MI)` with forged `MI+0x68`: 1 write (`[E+0x1829C4]=0`), then faults in `0x140A0C3E0` on `[MI+0xE0]=0`. The analyze struct's `+0xE0` (and the real `+0x68`) are also transient. CONFIRMED (bigest.py).

## 4. Determinism

- consume.py twice on rec0: identical (40 calls, census 625, `exact: 0`, same `man0pos`, same per-section `bad` pattern). Pump dispatch twice: identical traces. CONFIRMED 3+ identical runs of the consumer path (Advance's H-step det3 + these two).
- No frame-advance runs exist to compare (nothing advanced). Stated plainly per the ticket: the "3 identical runs" criterion is met only for the consumer-only paths.

## 5. The precise missing piece (why the match can't advance in emu)

To fire `0x140A128E4` (`inc [ES+0x1198]`, offsets[5]/type-13 body) the emu needs, in one instant, all of:

1. A **fresh type-13 record** with `[0x24]==[8]-[4]`, `[0x31A]!=0`, and roster words matching the *current* frame (`0x140A0E690<=0x15` gate). Every snapshot body is stale (roster moved on) or spent (`31a=0`). The builder of fresh type-13s (which of the 6 producer call sites, and its `+0x31A` source) is unidentified — static disassembly of the 6 sites' builders is the next step, no live data needed. OPEN.
2. The **per-frame transient bindings**, all null/empty at rest in 7/7 snapshots and ~30 live polls: `[D+0xB600+8·i]` detectors (223/223 null), the analyze-listener object (no heap instance: vtable `0x14259A350`/fn `0x14042B9F0` have zero image refs and zero heap hits — runtime-constructed like the hub), its `+0x20` struct (`MI+0x68`/`+0xE0` are 0 in every snapshot). These are built by analyze steps 1–5 + H each frame and torn down after. A SIGSTOP snapshot *during* the ~17 ms frame window (or a setup-window capture per S4 §5–§6) would supply them; ~10 s of polling never landed one. OPEN.
3. The **publish link** `0x14100CCA0` (pdata `0x14100CCA0–0x14100CCC6`: copies `+0x1188…+0x11A0` incl. `+0x1198` from `rdi` to `rbx`), called from `0x1410D97A6`/`0x1410D9942`. This is how the E-side counter becomes `MI+0x1198`. Driving it needs its `(rcx, rdx)` identities (stride `0x11A8` suggests a table, not MI directly). OPEN.

No second thread's *work* is missing (the pump thread only feeds input sync); no host callback or device is missing (all faults are null/transient-state reads, all import models resolved except a logging path). The missing piece is **snapshot timing + the type-13 builder identity**, both obtainable statically or with tighter snapshot polling — evidence above.

## 6. Writers

Bounds from `.pdata` (CONFIRMED). "Call stack" = static caller chain CONFIRMED by disassembly + the emu path that reaches each site.

- **Frame counter**: `0x140A128E4` (`inc dword [rsi+0x1198]`, `rsi` = E-side struct `E+0x180C60`) in offsets[5]/type-13 body (func `0x140A123AB–0x140A12B60`, reached via `0x140A0D190+0x1CA` → `0x140A13060` → `0x140A115A0` → idxbyte-5 case; gated by `[rec+0x31A]!=0` at `0x140A128D4`). Init/zero writers: `0x1408AE77E` (func `0x1408AD0B0–…`), `0x1408B3F2C`, `0x140A3BAC7`, `0x140A3C59E`. Publish to MI: `0x14100CE6C` (`mov [rbx+0x1198],eax`, func `0x14100CCA0–0x14100CCC6`, callers `0x1410D97A6`, `0x1410D9942`). Sibling E-side writers in the same body: `0x140A12851` (`[+0x118C]+=edi`), `0x140A12857` (`[+0x1190]++`), `0x140A128DE` (`[+0x1194]+=edi`); type-9 body `0x140A11A85` (`[++0x1180]`, func `0x140A1160F–0x140A11ACB`). CONFIRMED (static scan + gate traces).
- **Positions** (`movss`, CONFIRMED static opcode scan, SIB-aware): `man+0x55C` (x): `0x14168023A` (func `0x14167FE60–0x1416805E6`, init-from-`.rdata` block writing `+0x548…+0x564` + `[+0x5B0]`), `0x141988598` (func `0x141987A20–0x1419888BB`, copy-from-`[rdi+…]` block writing `+0x555/+0x558/+0x55C/+0x560/…`), `0x141B88CC8` (func `0x141B88C90–0x141B88CEA`, conditional write after `ucomiss [rcx+0x55C]/[rcx+0x560]`, source `xmm1/xmm2` args). `man+0x564` (z): `0x14168025A`, `0x1419885C6`. `man+0x560` (y): `0x14168024A`, `0x1419885AF`, `0x141B88CD0`. `man+0x578`: `0x140A85A49`, `0x140A85A83` (funcs `0x140A85A40–…`/`0x140A85A70–…`). `man+0x574`/`+0x57C`: no `movss` store anywhere (loads only) — likely written as vector (`movups`) or computed; OPEN. No per-frame call-stack attribution (which writer runs per frame is dynamic; none fired in emu). INF placement, CONFIRMED sites.
- **Ball**: no `movss`/`movups` store to `MA+0x2482C`/`+0x24834`/`+0x2601C` image-wide (CONFIRMED scan) — the ball cluster is written through a computed base (same phenomenon as S4's list). OPEN, same remainder as S4.
- **S4 nearest-list writer**: still OPEN. Eliminated this pass: the entire pump-consumer path (routers/queues/drain/key consumers — all run clean with zero MA writes), the detector-consume path heads, `0x140A22130` (E-scratch only). Remainder per S4 + this pass: the analyze-listener family bodies (`0x140426760`/`0x140A0D190` callees `0x140A20020/0x140A21850/0x140A0E940/0x140A32CF0/0x140A0E6D0/0x140A32B90`, `0x140A2D800`, `0x140A1F1F0`/`0x140A1D110`/`0x140A33540`/`0x140A343E0` tails) and the `0x1404F8C70` impure region. Next step: disassemble those callees for `imul 0xC8` + float-`d²` + count/rank stores, or a write-hook run once a live-frame snapshot (§5.2) exists.

## 7. Unresolved / blockers (ranked)

1. Type-13 builder identity (which producer call site sets `[rec+0x31A]`; static, no PES needed).
2. Live-frame snapshot (transient bindings: detectors, analyze obj, `MI+0x68/+0xE0`; needs tighter polling or a setup-window capture).
3. `0x14100CCA0` caller args (`rcx` table stride `0x11A8`, `rdx=rbp-0x40` source identity).
4. S4 writer (remainder above).
5. `man+0x574/+0x57C` + ball-cluster writer (computed-base stores; same hunt as 4).
6. H-vs-analyze order + thread attribution (needs owner-approved timing, not just reads).

## 8. Recommended next step

Disassemble the 6 producer call sites' record builders (esp. `0x140A0E0D3`'s `0x140A867D0` chain and `0x140A0E486`'s `vt+8` chain) to find which builds type-13 with `[+0x31A]!=0`; forge that record against rec2's roster and re-run `forge13.py`. In parallel, poll `MI+0x68`/detector slots at <100 ms cadence for a live-frame snapshot. Either unblocks the first emulated `inc`.
