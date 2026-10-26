# PES EmuWatch: nearest-list writer hunt + hub tables + queue order (EmuWatch, 26-10-26)

Read-only RE. Snapshot `snap1-142012` (`<workdrive>/gf-FullSnap-scratch/snap/`,
`G=0x392FA2D0 MI=0x1BC04608 frame=30381 per=3`); emulator = FullSnap `frame_trace.py`
(`origin/FullSnap fccd8471`, copied into scratch — never edited TracedCases' worktree).
Image `pes_full.bin` (offset = VA - 0x140000000). No PES bytes quoted.
Labels: **CONFIRMED** = emulator/snapshot bytes read this pass; **INF** = inference; **OPEN** = blocker named.
Scratch: `<workdrive>/gf-EmuWatch-scratch/` (`emurun.py`, `queue_order.json`, `runlog.json`, `writes.json`).

Chains re-verified in snapshot: `M=*(0x1436F3F78)=0x1F78EC80` (vt `0x142596DC8`),
`H=*(0x1436F3F98)=0x2C69A410` (vt `0x142597580`), `W=*(M+0x118)=0x0A3EFD70` (vt `0x14259D4F0`),
`blk=*(W+0x10)=0x7FF5FD3B0010`, `MA=blk+8=0x7FF5FD3B0018`. List span
`MA+0x22E4C..+22*200 = 0x7FF5FD3D2E64..0x7FF5FD3D3F94`, fully readable.

## 0. Headline verdicts

| # | Ticket item | Verdict |
|---|---|---|
| 1 | S4-WRITER (VA + call stack + frame position) | **CONFIRMED outside the H tick.** 50 emulated H frames: 0 writes to the whole MA object (not just the list span); list bytes bit-frozen (`fb15333c…`). Writer is NOT in the tick path. Caller: narrowed to pump/analyze-side, not yet entered (see §3). |
| 2 | Hub `+0x1820` default | **CONFIRMED contents** at hub `0x2C6945C0` (see §4); "default" reading: unregistered slots hold small-int/hash junk (`0x23` at t=3 both tables), i.e. no valid listener default — dispatch of such a type would fault, so those types never dispatch on this hub. |
| 3 | Dispatch queue order | **CONFIRMED producer order**: 84-node doubly-linked queue walked from the snapshot (§5); first 50 nodes alternate per-side/per-player listener records (seq `0x10402000D…0x103D`), then H/referee/analyze/player records. Per-record type/channel words stay OPEN (record-body decode, §5). |
| 4 | Stage table update | §6. |

## 1. The 50-frame watch (method + controls)

- Entry: `H step 0x14041ECF0(rcx=H)`, instruction budget 50 M/frame (deterministic guard,
  FullSnap convention), lazy snapshot mapping, 4 active stub models only
  (`__security_check_cookie`, abort-trap, sqrt/powf thunks) — the whole tick body runs image bytes.
- Hook validation (CONFIRMED): a synthetic `mov [rax],rbx` to a lazy-mapped MA page fires the
  `UC_HOOK_MEM_WRITE` hook exactly once with the right addr/size; hook works with `install_hooks`
  active and with up to 3 overlapping range hooks.
- Hook-combination trap found and avoided: `UC_HOOK_MEM_READ`+`UC_HOOK_MEM_WRITE` share one
  callback namespace in the probe scripts — an early "2521 MA hits" reading was **reads**
  (re-split: 463 distinct read sites on/near the list, 0 write sites). All writer counts below
  come from **write-only** hooks.
- Per-frame result (5-frame `runlog.json` + 50-frame `sniff.py` run): `ticks=2/frame`
  (`0x1404F6C30` × 2), `err=null`, `list_before == list_after == fb15333c…` every frame,
  `list_changed=false` all 50. Full-MA-object write hook (`MA..MA+0x27000`): **0 writes** while
  208,109 total writes land elsewhere (top pages: emulator stack + H/team/MI heaps).
- The run DOES reach the MA chain: landmark coverage per H step — `0x1404F6C30:2`,
  `0x1404F7955:2`, `0x140535C60:2` (MA build), `0x140536FC0:1`, `0x140538050:2` (mode dispatch),
  `0x140537AC0:2`, `0x14053C6E0:2`, `0x140579840:2`, `0x1404405D0:1`, `0x1405069A0:1`,
  MA-chain callees `0x140536690:2`, `0x140534A20:2`, `0x1405346E0/315490/31260/31810/325E50/
  52F4F0/52F720/532F70/30320/36210/634470/635AB0/628400:1`, `0x1404F8C70:2`.
  Not reached: `0x140535A10/535350/535650/5360D0/536170/5379C0/537BF0/5387A0-dispatch-arms/
  539260/539AF0/538160/539860` (0 hits) — the sort/concept arms and the dispatch reader arms
  that would consume a freshly built list.
- MA reads during the tick (top offsets): `+0x2601C:3172` (carrier), `+0x25D08/0C/10/14`,
  `+0x2482C:505` + `+0x24834:382` (ball cluster), `+0x25CF8…`, `+0x22BA4:117`,
  `+0x23874:32` (inside the list span — the tick READS the list 149× via `0x1404FEA76`-family
  sites `0x7FF5FD3D4858/…B0`, `0x140A7152E→+0x228D4`, `0x1404FF10C/13D/145/544→+0x2482C/40/48`).
  So the tick consumes list + ball cluster built by a PREVIOUS frame (lag ≥ 1, consistent with
  S4 §4's lag-1 finding), but never refreshes them.

## 2. Widened entries (all write-watched on the list span)

| Entry | Args | Result |
|---|---|---|
| MatchSub slot 2 `0x140433760` (rcx=`0x2C6A9020`, the H-bearing MO listener; forged `[rec]=2`) | type-2 path → gate → H ×2 → teardown, clean return (`last rip 0x140426D88`) | **0 list writes (CONFIRMED)** |
| Analyze slot 2 `0x14042B9F0` (rcx=`0x1C82E6D0`, the analyze listener) × 9 forged types (`2`, `0x1050012/001/00F/00E/02C`, `4`, `1`, `0x23`) | all reach only step-10 switch `0x140426CD0`, clean return `0x140426D88` | **0 list writes (INF — forged records: the `[rcx+0x40]` delegate pre-switch likely validates; real queue-node records carry Q1/Q2 context the forgeries lack)** |
| MA-build `0x140535C60` direct (captured tick regs: rcx=`0x2C69EFD0/0x2C6A5DD0`, rdi=`T_side`, r8=`MI+…`, rbx=H) | fault `0x140533D53: [r15+0xE0]` (needs live tick stack: `[rsp+0x68/70/78]` layout) | **inconclusive (OPEN)** — needs call-site emulation from `0x1404F7945`, not bare entry |
| Per-player slot 2 `0x140925750` (rcx=player0 `0x3D5CBF30`) | fault `0x1403EFE17: read 0x0` (needs record/queue context) | **inconclusive (OPEN)** |

## 3. S4-WRITER verdict

**CONFIRMED: the 22-row `{id,d²}+counts+ranks` builder does not run in the H step, the MatchSub
type-2 path, or (within forgery limits) the analyze steps.** The list is frame-constant across
50 emulated ticks while readers consume it every tick — it is built outside the tick, i.e. the
caller is above the tick: the pump dispatch (`0x141B3DD80→0x141B3EA30→0x141B3EDD0`) delivering a
non-type-2 record, or a setup/phase-change path (mode dispatch arms never ran here; the match is
in open play, `per=3`, frame frozen at 30381 — `MI+0x1198` is copy-refreshed, not ticked).
Next step (named): emulate from the pump with hub+record, or catch a phase change (set-piece —
S4 §7 item 3's tie data comes from the same capture); the forged-analyze path needs the real
queue-node record body (Q1/Q2 objects at node+0x10/+0x18), not a bare type dword.
`// ponytail:` writer-unknown marker for S4 stays.

## 4. Hub tables (CONFIRMED snapshot reads)

Hub `H_hub=0x2C6945C0` (vote peak over 155 listener slots; aliases at ±0x1800 are the same
physical tables — `hubA+0x1820 ≡ hubB+0x20`, resolved in favour of hubB by the `+0x820/+0x1020`
junk test):

- `+0x20` t=8..29 = `match::Player` objects idx **0..21 in order** (t = 8+idx); t=0..7 =
  `[PL8, hash, hash, 0x23, PL8, hash, hash, 0xF3]`; t=30+ = spillover junk.
- `+0x1820` t=19..40 = players idx 0..21 (t = 19+idx); t=0..7 =
  `[PL17, hash, hash, 0x23, PL17, hash, hash, 0x163]`; t=3 = `0x23` in BOTH tables.
- `+0x820`/`+0x1020` at player t = engine billboard objects / junk (never listeners) — so
  player records dispatch on the plain path (`+0x20` then `+0x1820`, both = the player;
  slot 2 runs twice per player record).
- Default reading: unregistered slots (t=3: `0x23`) hold non-pointer junk. The dispatcher calls
  both holders unconditionally (`0x141B3EEB5–3`, no null checks), so a type whose `+0x1820`
  holder is junk can never dispatch on this hub — the `0x23` slots mark never-registered types,
  not a usable default. Register arm `vt+0x30` writes `+0x20` only (open doc §2.1 stands);
  who wrote players into `+0x1820` (ctor copy vs `vt+0x38` outside the registrar region) is OPEN —
  zero direct E8 callers of arm `0x141B3DB40` image-wide (virtual-only, like all hub calls).
- Live values (snapshot, CONFIRMED): `[M+0x100]=0x4326000004020000` (type-base word; low dword
  `0x04020000` matches S4 §5), divisor `[0x1434456B8]=0` in play, player `+0x10` counters =
  frame (30381) for 20/22 (PL1/PL21 hold `4294997677` — alternating-tick pair, INF).

## 5. Per-frame queue order (CONFIRMED chain walk)

84-node doubly-linked queue (`+0x0` next / `+0x8` prev), walked from `0x2C6C5960`
(`queue_order.json` in scratch):

- Nodes 1–50: strict alternation, seq `0x10402000D…0x10402003D`: odd seqs target
  MatchObjectListeners (vt `0x14259B1A8`, one player each — the per-side/per-player listeners),
  even seqs target family `F2` (vt `0x142596DE8`, slot 2 `0x14041B840`, `+0x20=0x1F` count word).
  I.e. producer order = per-player records interleaved across two listener families.
- Then: H-bearing MO (`0x2C6A9020`, seq `0x35`), referee-ish (`0x14259A150/130`-vt holders,
  seq `0x36–0x3B`), tail MO (`0x2C6C4DB0`, `0x3D`), misc engine records (seq `0x1000000+`,
  `0x104050000`, `0x101000000` — cross-map links, pump-external queues), engine freight
  (seq `0x80–0x8C`), the **analyze record** (`0x391BCB00 → AN 0x1C82E6D0`, seq `0x8D`,
  LAST of its run), then a wrapped run seq `0x1–0xC` (second-queue head region).
- Payload note: node+0x58/+0x78 = `0x23` in 70/84 nodes (record-family tag, INF — matches the
  referee `0x23` record family, not the dispatch type); the dispatch type dword lives in the
  queue-object records (`[rax+rbx]` slots), whose container Q is still OPEN (stride candidates
  ambiguous). So: **order CONFIRMED, per-record type/channel mapping OPEN** (needs the record-body
  decode or a live pump run).

## 6. Stage table update (plan §4 terms)

- **S0** — unchanged (needs live/setup captures; this work used frozen open-play snapshots).
- **S1** — unchanged; note: driver head `0x140A48A30` ran per player per frame in the runs above.
- **S2** — unchanged (block allocator OPEN-1 stands).
- **S3** — unchanged (words CLOSED per IntegrationOpen §4; frame word `1198` CONFIRMED frozen
  across H steps here — copy-refresh lives outside H, consistent with `0x14100CCA0` writer).
- **S4** — **still BLOCKED, narrowed to off-tick.** Ships as known-approx writer. The exact
  builder is NOT in: H body, MatchSub type-2 path (CONFIRMED), or forged analyze steps (INF);
  next: pump/phase-change entry (named in §3).
- **S5/S6** — unchanged. Note: `0x1404F8C70` ran twice/frame with zero MA writes (its impure
  region is not the list writer either).
- **S7** — FLAG narrows: H skeleton order re-CONFIRMED by coverage (§1); frame-counter evidence
  added (H never advances `MI+0x1198` — S7's frame bind lives outside H).
- **S8** — unchanged (chain shippable; direct entry needs record context — fault named).
- **S9** — referee/queue evidence extends the placement case: per-frame queue order CONFIRMED
  (§5); hub tables CONFIRMED (§4); remaining: record type/channel decode + dispatch-hub proof
  (t=3 junk-holder caveat).

## 7. Open items (ranked)

1. `S4-WRITER`: off-tick builder VA (pump entry or phase-change path; analyze forgeries need real record bodies).
2. Record type/channel decode (queue-object Q container) — blocks per-record dispatch mapping.
3. Dispatch-hub proof (enter `0x141B3EDD0` with hubB + real record) + who fills `+0x1820` (ctor vs `vt+0x38`).
4. Tie rule on exact-`d²` ties (unchanged — no ties in this data either).
5. PL1/PL21 counter `4294997677` meaning (alternating-tick pair? one snapshot sample, INF).


## Main's note

The emulated H tick does not advance the match: across 50 frames `MI+0x1198` never moves and MA's list stays frozen, because world state (positions, the nearest-list) is written outside H, by the pump dispatch (`0x141B3EA30`). Emulating H alone repeats one frame's world 50 times: coverage from it reflects one situation per snapshot. The real frame entry is the pump.
