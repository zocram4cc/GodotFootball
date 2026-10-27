# PES RE: cutover §5 open items — S3 / S8 / S9 (OpenItems, 27-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file-offset = VA − 0x140000000).
Labels: **CONFIRMED** = disassembly bytes read this pass (capstone over the image, `mmap`); **INF** = inference; **OPEN** = narrowed remainder with the exact next step named.
No PES bytes quoted. PES pid <pid> alive (polling not needed — all claims static; no SIGSTOP taken this pass).
Prior docs re-checked, not re-derived: `pes-re-integration.md` (§4/§5), `pes-re-integration-open.md` (IntegrationOpen 26-10-26 — **two errors corrected here, §6**), `pes-re-tick-order.md` (MB-H), `pes-re-record-layouts.md` (RL §4), `pes-re-liveroster.md`, `pes-re-s7-records.md`.
Scratch: `<workdrive>/gf-OpenItems-scratch/`.

## 0. Headline verdicts (per ticket item)

| # | Item | Verdict |
|---|------|---------|
| 1 | Hub Register `vt+0x30` — registration + dispatch order | **CONFIRMED** (was top OPEN). Two prior-doc errors corrected: `vt+0x38`/`vt+48` VAs were swapped; match region uses **only** `vt+0x30` → `+0x20` table (INF on hub `+0x1820` runtime default). |
| 2 | `match::Player` slot-2 loop caller (S8, team-vs-player order) | **CONFIRMED** as hub record delivery, per-player type index, order 0→21. **Divisor VA corrected**: `0x1434456B8`→**`0x143445EB8`**, static value **1** → `counter%1==0` always, so **every player ticks every frame** (no side gating in the static build). |
| 3 | Per-tick MI/roster binder set/clear (S3 liveness, S7) | **NARROWED.** `G+0x180` read pinned (type-13 body `0x140a115e9/0x140a11b52`); the **set is not a direct `[base+0x180]` store anywhere in the match region** (image-wide scan: zero 64-bit stores, zero `add/lea` computed-base forms) → it is an aliased/computed write inside the ~17 ms analyze window (LiveRoster EV-LR-0002 stands). MI refresh `0x14100cca0` + 6 distinct caller fns; the per-tick one narrowed to the adjacent pair `0x14113b27f`/`0x14113b51f` (both call the copy). |
| 4 | `T+0x78` writer (PASS_GET / kick path) | **CONFIRMED** per RL §4 (no change): `T+0x78 := F0E50(...)`; chain `4FB2A0 → 4FB380 → F0E50`. `F0E50` stores `T+0x6D00` (winner); the caller of `FUN_1404FB380` (fn `0x1404fb29f`) stores `T+0x78`. Independently ported (lane-leaves). |
| 5 | Referee / animation / camera callers for S9 | **CONFIRMED** (IntegrationOpen §2.3–2.4 stands, re-verified): referee event-driven via own record type (whistle→`MI+0x1775`→restart-branch); camera + pose render-side (zero E8 to `0x14087f280`); MA build-then-animate order = tick-then-render. |

---

## 1. Hub Register `vt+0x30` — registration + dispatch order (S9, GF `Match::Process` order)

### 1.1 Register arms — CONFIRMED, image vtable read (correction)

Hub vtables `0x142ac1cb0/1d48/1de0` share the Register slots. Reading the qwords **directly this pass**:

| slot | VA | table written | store (disasm) |
|------|----|---------------|----------------|
| **`vt+0x30`** | **`0x141b3db70`** | `+0x20` (main/default) | `mov [rbx+rsi*8+0x20], rdi` @ `0x141b3dbe0` (plain overwrite; old released via `[rax+8]` @ `0x141b3dbed`) |
| **`vt+0x38`** | **`0x141b3dc90`** | `+0x820` (channel-2) | `[rdx+0x820]` |
| **`vt+0x40`** | **`0x141b3dc50`** | `+0x1020` (channel-0) | `[rdx+0x1020]` |
| **`vt+0x48`** | **`0x141b3db40`** | `+0x1820` ("always") | `mov [r9+rax*8+0x1820], r8` |

**Correction to IntegrationOpen §2.1:** the prior doc paired `vt+0x38`↔`0x141b3db40` and `vt+0x48`↔`0x141b3dc90`. The vtable says `vt+0x38 = 0x141b3dc90` (→`+0x820`) and `vt+0x48 = 0x141b3db40` (→`+0x1820`). The prior doc had the two VAs **swapped** (the table-mapping per function was right; the slot label was wrong). No table mapping changes, so dispatch conclusions hold — only the slot labels do.

### 1.2 Who registers where — CONFIRMED
- The **match registrar region `0x140418000–0x14041a000` calls only `vt+0x30`** (16 `ff 50 30` sites; zero `0x38/0x40/0x48`). → **every match listener registers into the `+0x20` table at its type index `t`.** The channel-0/2 (`+0x1020`/`+0x820`) and `+0x1820` tables are **untouched by the match path.**
- The `+0x38`/`+0x40` Register calls all sit in a **separate non-match registrar** (`0x140411xxx`, garbled `CreateDXGIFactory` strings) — presentation/system-level, not the match tick. So the `+0x820`/`+0x1020` channel tables do not participate in the match frame order.
- Registration arms (names CONFIRMED strings, IntegrationOpen §1.3): `"MatchMainListener"`/`"MatchSubListener"` (match path), `"PlayerListener"` (`0x142593c48`, the per-player), `"post_collision"` (its pair), `"RefereeListener"` (`0x142593c58`), `"match_begin"`/`"match_end"`/`"analyze"` (analyze registrar `0x140419250`).

### 1.3 Dispatch order per record — CONFIRMED (`0x141b3edd0`)

Per record: `low7` = type `t`, `bits7–16` = channel. Type 1 = pointer-follow; type 3 = end. Else:
- channel 0 → `[hub+0x1020+t*8]`; channel 2 → `[hub+0x820+t*8]`; **else → `[hub+0x20+t*8]`**;
- each `call [rax+0x10]`;
- **then always** `[hub+0x1820+t*8]`, `call [rax+0x10]`; next record.

For a type-2 match record: the **`+0x20` holder (the match listener) first, the `+0x1820` holder second — no null checks.** INF: the `+0x1820` slot is non-null at runtime (hub-ctor default or outer pump hub); the match hub object `M+0x10 = 0x37de64c0` was not captured in the prior snapshot. **Named remainder (INF): one live read of the hub object `+0x1820` slot** to confirm the default contents — does not block porting.

---

## 2. `match::Player` slot-2 loop caller (S8 — team-vs-player order)

### 2.1 Chain top — CONFIRMED (re-verified this pass)

- `match::Player` vtable `0x1426329a8` (RTTI in A4TeamTick): **slot 2 = `0x140925750`** (re-read, matches). Full slot dump: 0 `0x1409210d0`, 1 `0x1402297e0`, **2 `0x140925750`**, 5 `0x1409250a0`, 8 `0x1409f1b50`.
- Wrapper `0x140925750`: `inc [rcx+0x10]` (frame counter) → `call 0x1408bf090` → **`call 0x140925470` unconditional** → `%`-gate → `call [rax+0x40]`. **Zero direct E8 callers** (whole-code scan, prior) — reached only by virtual dispatch `call [rax+0x10]`, i.e. **hub record delivery, one record per player per frame.**
- Driver `0x140925470` args = **self-only** (hub msg/dt discarded); team re-fetched per player: `side fold` → `call 0x140a48a30(G, side)` = `T_side`.

### 2.2 Order over the players + divisor — CONFIRMED (divisor VA corrected)

- Registrar `FUN_0x140418940`: 22-iteration loop; per player `eax = counter++`, **`div eax, [0x143445EB8]`** (`F7 35 7D D4 02 03` @ `0x140418a35`, RIP-relative → **`0x140418a3b + 0x302d47d = 0x143445EB8`**). If remainder == 0 → `FUN_0x140418710` (MatchObjectListener) + `FUN_0x140419a10` (paired `"post_collision"`) register at type index `edi`, `edi += 2`.
- **Correction:** IntegrationOpen §1.3 named the divisor `0x1434456B8` — that is **off by 0x800**. The true RIP-relative target is **`0x143445EB8`**, static value **`1`**. → `counter % 1 == 0` for **all** 22 counters → **every player ticks every frame** (no side-1 gating in the static build). Per-frame delivery order = ascending type index = **global player order 0→21**.
- Owner array (CONFIRMED, prior): `M+0x10[0..21]` = 22 Player ptrs; `M = 0x1f230710` vtable `0x142596dc8`.

**S8 consequence:** the team tick and the 22 player updates are **separate hub records** (the team tick inside the match listener's type-2 record; each player at its own type index). Their **relative order = the producer-side queue order of those records** (§4 sequence diagram). Porting the chain is unblocked; the divisor/type-base no longer gate which players tick (divisor = 1).

---

## 3. Per-tick MI/roster binder — set/clear + in-frame position (S3 liveness, S7)

### 3.1 The `G+0x180` roster link — CONFIRMED read, NARROWED set

- **Read (CONFIRMED this pass):** the type-13 frame-advance body `0x140a115a0` reads it:
  - `0x140a115e9: mov r13, qword ptr [rax + 0x180]` (rax = G = `*[0x1436F3DC8]`) — this is the `r13` roster-link LiveRoster §4.1 needs.
  - `0x140a11b52: mov rbx, qword ptr [rax + 0x180]` (second use).
  - With `r13 = 0`, `0x140a0e690` scans `0+k·0x74+0x10` → `0xFF` → clean exit (LiveRoster EV-LR-0002/0003).
- **Set (NARROWED):** image-wide scan of all `[base+0x180]` **64-bit stores**: zero in the match region `0x14040000–0x14300000`; zero `add r?,0x180` / `lea r?,[r?+0x180]` computed-base forms in the same region. → **the set is not a direct-displacement store; it is an aliased/computed write** that lives only during the analyze-listener step window (LiveRoster: set in step 5 `0x140426760`, torn down before any STOP; the ~17 ms window is why `G+0x180 == 0` at every snapshot).
- **Clear:** no matching standalone clear site either; teardown is the analyze-window release (LiveRoster §4). The "match setup/teardown" G-table writers TeamTick §4.1 named (`FUN_140a48210`, `0x140a49c3b`) are **not** the `+0x180` writer (disassembled this pass: `FUN_140a48210` has no `+0x180` store and no G-global load — it is a different G-table op).

### 3.2 The MI whole-struct refresh (`0x14100cca0`) — CONFIRMED caller set, NARROWED per-tick

- `0x14100cca0` copies `1198/119c/11a0` (the MI phase-word refresh, GapsA2 b3 shape). **7 call sites → 6 distinct enclosing fns** (re-derived this pass):
  - `0x1410d907f` (fn), `0x1410d988f` (fn), `0x14110fdff` (fn), `0x14113b27f` (2 calls), `0x14113b51f` (1 call), `0x1414e178f` (fn).
- **None has a direct E8 caller** in the match region and **none sits in the match-object vtable** (`0x142596dc8`) → all six are **vtable methods of other objects** (dispatched, not called). Two adjacent fns in the `0x14113bXXX` block — `0x14113b27f` (calls `0x14100cca0` twice) and `0x14113b51f` (once) — are the **per-tick refresh candidates** (they are the only pair in the same function cluster; the other four are isolated).
- **OPEN remainder (narrowed):** which of the two cluster fns runs per tick, and whether the `G+0x180` aliased set is the *same* window or a distinct one. **Next step:** a `<1 ms` SIGSTOP poll of `G+0x180` + `MI+0x1198` across one frame (PES is up, pid <pid>) — or a write-hook on the translated step (S7) — to catch the set/clear cadence. Static byte-scan cannot pin it (no direct store form exists).

**S3 consequence:** the 8 MI phase-words still have per-word writers/callers (IntegrationOpen §4 table stands). The binder is **not a blocker** for S3 liveness: `IsInPlay`←`116c/1775`, frame←`1198` (copy-refreshed, writer `0x14100cca0`), etc. S7 FLAG narrows to the `G+0x180` set-window (above) — the PES step itself runs on PES-layout state, so the writer executes as translated code once S7 lands.

---

## 4. `T+0x78` writer (PASS_GET / kick path) — CONFIRMED (RL §4, re-verified)

- `T+0x78` = per-side u32 holder id (0..21, `0xFF` = none). **`FUN_1404FB380`** (fn head `0x1404fb29f`, the **caller** of the F0E50-family winner `0x140a3e190`) is the store site; the F0E50 winner `0x1404f1480`-family returns the id and `F0E50` itself stores `T+0x6D00` (winner, `0x1404f4677 MOV [RSI+0x6D00],R14D`).
- Chain **`4FB2A0 → 4FB380 → F0E50`** (lane-leaves §4, independently ported). Re-confirmed this pass: `0x1404fb368: call 0x1404fb380` sits inside the kick-path fn; `0x1404fb413/0x1404fb427: call 0x140a3e190` (the winner-compare).
- **No action** — closed by RL §4; the ported `kickMan/kickValid` leaves match.

---

## 5. Referee / animation / camera callers (S9) — CONFIRMED (IntegrationOpen §2.3–2.4, re-verified)

- **Referee (event-driven, not ticked):** vtable `0x142598558` slots 0/2/3 (`0x140422030`/`0x140420f8c`/`0x140423000`). Chain: slot 0 `0x140422030` → `0x140445e90` → `0x14044c880` → outer record switch `0x14044c7b0` (codes `0x3b–0x42`) → judge switch `0x14044a180` (codes `0x29–0x31`) → judge fns → **whistle `0x14050e770`** (5 E8 callers, all judge-side). Slot 0 `0x140422888` calls `0x140a3cae0` = **`MI+0x1775` mode writer** → restart-branch (`MI+0x1775==0x17` in team-tick steps 20–23). **"RefereeListener" exists** (`0x142593c58`) → the referee is hub-driven via its own record type, ordered by queue position. **Wire order: whistle → `MI+0x1775` → restart branch. CONFIRMED.**
- **Animation + camera (render-side):** `InplayCamera::update` `0x14087f280` — **zero E8 callers image-wide** (re-confirmed) → virtual/render path, per-rendered-frame cadence. MA consumers (`0x140530320/532f70/587100/5b89f0/5e6e80`) run **inside the team tick** (MA chain under `0x1404f6c30`); pose/camera consume the built MA **after** the tick. **MA build-then-animate order = tick-then-render. CONFIRMED.**
- **Referee-object ctor:** still OPEN (heap-built, zero image holders) — placement does not need it.

---

## 6. Corrections to IntegrationOpen (26-10-26)

1. **`vt+0x38`/`vt+0x48` Register-arm VAs swapped.** Correct (vtable read): `vt+0x38 = 0x141b3dc90` (→`+0x820`), `vt+0x48 = 0x141b3db40` (→`+0x1820`). Table-mapping per function unchanged; only the slot labels.
2. **Divisor VA off by 0x800.** `0x1434456B8` → **`0x143445EB8`** (RIP-relative recompute of `F7 35 7D D4 02 03` @ `0x140418a35`). Static value **1** → all 22 players tick every frame (no side gating in the static build).

## 7. Frame-order sequence diagram (hub listener, team tick, player update)

Producer side: the match path builds one **per-frame tick record** (type 2) whose **sections are filled in a fixed code order before the push** (push `0x141b3e060`; record ctors `0x141b3f0d0`/`050`). The **pump** (hub vtable slot 6 `0x141b3dd80` → `0x141b3ea30`) drains one queued list per call (GapsA2 2-frame lag, counter `hub+0x4190`). **Per-frame order across subsystems = record queue order (producer order), not registration order.** Within one type: the `+0x20` holder first, then the `+0x1820` holder.

```
                 ┌──────────────────────────────────────────────────────────────┐
                 │  PRODUCE (match path, fixed section order, before push)      │
                 │  type-2 tick record:  [match-listener section] [+ per-player]│
                 └──────────────────────────────────────────────────────────────┘
                                              │  push 0x141b3e060  (queue, 2-frame lag)
                                              ▼
   PUMP   hub vt slot6 = 0x141b3dd80 → 0x141b3ea30   drain one list / call   (counter hub+0x4190)
                                              │
                                              ▼
   DISPATCH  0x141b3edd0  (per record):  low7=type t, bits7-16=channel
                                              │
        ┌─────────────────────────────────────┼─────────────────────────────────────────┐
        ▼ type2 (match listener)              ▼ per-player type index (0..21)          ▼ always
  +0x20 holder = match listener        +0x20 holder = PlayerListener i             +0x1820 holder
  vt 0x14259a350 slot2 = 0x14042b9f0   vt 0x1426329a8 slot2 = 0x140925750         (default; INF)
  (10-step tick: steps 2→3→4→5→6)       (wrapper: inc[+0x10] → call 0x140925470)
        │  steps 2-6 run the TEAM TICK        │  driver = self-only;
        ▼  (MatchSub L vt 0x14259b1a8 slot2 = T_side re-fetched 0x140a48a30
  0x140433760 → FUN_1404334e0 →          per player; order 0→21,
  tail 0x140a8eff0 → H->vtbl[2]         divisor [0x143445EB8]=1 → all tick
  = 0x14041ecf0 (H vt 0x142597580)          │
  (team tick: loop both sides,                 ▼
   call 0x1404f6c30 per side)          0x140925470 player update
        │                                    (AI / movement / ball control)
        ▼
   MA build (0x1404f6c30 MA chain)  →  [tick done]  →  render side: pose + InplayCamera 0x14087f280
                                              (camera/pose run OFF-tick, after)
   REFEEER: own record type in queue order → judge → whistle 0x14050e770 → MI+0x1775 0x140a3cae0
            → restart branch (team-tick steps 20-23)   [event-driven, not ticked]
```

**Ordering facts (CONFIRMED unless marked):**
- **Team tick** = inside the match listener's type-2 record, steps 2–6, via MatchSub `L` (vt `0x14259b1a8` slot 2 `0x140433760`) → `0x1404334e0` → tail `0x140a8eff0` → `H->vtbl[2]` = `0x14041ecf0` (H vt `0x142597580`), which loops both sides and calls `0x1404f6c30` per side. CONFIRMED.
- **Player update** = hub record delivery at per-player type indices, order **0→21**, divisor `0x143445EB8` = 1 → **all 22 tick every frame**. CONFIRMED.
- **MA build** runs inside the team tick; **pose/camera** run after, off-tick. CONFIRMED.
- **Referee** = its own record type, queue-ordered; whistle→`MI+0x1775`→restart. CONFIRMED.
- **Team tick vs player update relative order** = the **producer-side queue order** of the type-2 record's sections. The team tick is in the match-listener section; the 22 player updates are in their own type-index sections. **The exact section order within the type-2 record is the one remaining INF** (producer-side; needs a live multi-record capture, S0). The port is unblocked: the match listener (team tick) and the player records are separate dispatches, so GF `Match::Process` = *team tick step, then the 22 player updates* in queue order, then off-tick render.

## 8. Remaining OPEN items (named, ranked)

1. **Producer-side section order** within the type-2 tick record (team tick vs the 22 player records) — the one ordering INF. **Next:** a live multi-record capture (S0 / RegCapture) or trace the record-ctor `0x141b3f0d0` section-fill order.
2. **`G+0x180` set-window + per-tick MI-refresh fn** (`0x14113b27f` vs `0x14113b51f`) — NARROWED, needs `<1 ms` SIGSTOP poll of `G+0x180`/`MI+0x1198` (PES pid <pid> up) or an S7 write-hook.
3. **Hub `+0x1820` runtime default** — one live read of the hub object `0x37de64c0` (not captured). Does not block porting.
4. **Referee-object ctor** — heap-built, zero image holders; placement does not need it.
5. **`MA+0x22e4c` nearest-list writer** — still OPEN (IntegrationOpen §3; off-tick/pump/phase-change; S4 ships as known-approx).
6. **Block allocator / unmodellable `1404F6C30`/`1404FEDC0`** — untouched (plan §5; translation pipeline owns).

## 9. Evidence IDs (this pass)

- EV-OPEN-0001 (CONFIRMED): hub vtable Register arms `vt+0x30=0x141b3db70`/`vt+0x38=0x141b3dc90`/`vt+0x40=0x141b3dc50`/`vt+0x48=0x141b3db40`; `vt+0x30` store `[rbx+rsi*8+0x20]=rdi` @ `0x141b3dbe0`.
- EV-OPEN-0002 (CONFIRMED): match region `0x140418000–0x14041a000` uses only `vt+0x30` (17 sites); `+0x38/+0x40` in separate non-match registrar `0x140411xxx`.
- EV-OPEN-0003 (CONFIRMED): divisor `div eax,[rip+0x302d47d]` @ `0x140418a35` → VA **`0x143445EB8`**, static value **1** (IntegrationOpen's `0x1434456B8` off by 0x800).
- EV-OPEN-0004 (CONFIRMED): type-13 body `0x140a115a0` reads `G+0x180` at `0x140a115e9`/`0x140a11b52`; zero `[base+0x180]` 64-bit stores and zero computed-base `+0x180` forms in match region → set is an aliased/computed write in the analyze window.
- EV-OPEN-0005 (CONFIRMED): MI refresh `0x14100cca0` 7 call sites → 6 distinct fns; per-tick candidate pair `0x14113b27f`/`0x14113b51f` (same cluster, both call the copy); all six dispatch-only (no E8 callers, not in M vtable `0x142596dc8`).
- EV-OPEN-0006 (CONFIRMED): `T+0x78` writer = caller `0x1404fb380` (fn `0x1404fb29f`) of winner `0x140a3e190`; `F0E50` stores `T+0x6D00`; chain `4FB2A0→4FB380→F0E50` (RL §4).
- EV-OPEN-0007 (CONFIRMED): listener vtable slot-2 map — match `0x14259a350`→`0x14042b9f0`; H `0x142597580`→`0x14041ecf0`; MatchSub `0x14259b1a8`→`0x140433760`; Player `0x1426329a8`→`0x140925750`.
- EV-OPEN-0008 (CONFIRMED): `InplayCamera::update 0x14087f280` zero E8 callers (render-side); whistle `0x14050e770` 5 judge-side E8 callers; `MI+0x1775` writer `0x140a3cae0` from referee slot-0 `0x140422888`.

## 10. Method note (for the next agent)

Ghidra bridge `:8080` unavailable this pass (as last session); all evidence is capstone-over-image (`offset = VA − 0x140000000`) with `mmap` reads. **Do not `read()` the 462 MB image into eval memory** (2 GB `ulimit` kills the cell); `mmap` + chunked scans work. Whole-code E8 / byte scans take ~2–50 s; chunk at 32 MB. `head()` = nearest `CC CC` (verify against known neighbours — the `0x1404fb2dd` spurious-`ret` is the failure mode: a `ret` inside data, not a fn end). For offset stores, scan **both** the direct `[base+disp]` form **and** the `add/lea` computed-base form; a missing direct-displacement store means the write is aliased/computed (see `G+0x180`).
