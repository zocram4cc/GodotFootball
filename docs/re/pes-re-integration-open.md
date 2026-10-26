# PES RE: closing the cutover OPENs (IntegrationOpen, 26-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA − 0x140000000).
Labels: **CONFIRMED** = disassembly bytes or capture bytes read this pass (capstone over the image + `cap1/f01-open-p3-play1-fr30490` live.json/regions); **INF** = inference; **OPEN** = narrowed remainder with the exact next step.
No PES bytes quoted. Ghidra bridge (:8080) was down all session; all evidence is capstone disassembly + capture reads + the universe ledger.
Scratch: `<workdrive>/gf-IntegrationOpen-scratch/` (`dis_key.txt`).

Prior docs re-checked, not re-derived: `docs/re/pes-re-integration.md` (the plan, §1/§4/§5), `pes-re-tick-order.md` (MB-H), `pes-re-teamtick.md` (A4TeamTick), `pes-re-gaps-a2.md` (GapsA2), `pes-re-player-record.md` (PR), `pes-re-record-layouts.md` (RL), `pes-re-camera-target.md`.

## 0. Headline verdicts (per ticket item)

| # | Item | Verdict |
|---|---|---|
| 1 | S8 per-player driver (§1.4 OPEN) | **CLOSED** as chain + args + order (INF on exact per-frame record type; divisor VA named). Details §1. |
| 2 | S9 referee/restart/animation/camera (§1.5 + Register vt+0x30 OPEN) | **CLOSED** except animation-pose caller (render-side, named). Details §2. |
| 3 | S4 MA nearest-list writer (MA+0x22e4c) | **NARROWED, still OPEN.** Writer not found; ball-relative d2 sites excluded, candidates profiled. Details §3. |
| 4 | S3 MI phase-word writers | **CLOSED** per word (writer fn + role + caller; event-store lookalikes separated). Details §4. |
| 5 | Stage table update | §5. |

## 1. S8: the per-player driver — CLOSED

### 1.1 Chain top and wrapper (CONFIRMED, image bytes)

- `match::Player` vtable at `0x1426329a8` (RTTI already CONFIRMED in A4TeamTick; re-verified: slot 2 = `0x140925750`). Full slot dump this pass (image qwords):
  slot 0 `0x1409210d0`, 1 `0x1402297e0`, **2 `0x140925750`**, 3 `0x141794410` (stub), 4 `0x1409299a0`, **5 `0x1409250a0`**, 6 `0x1409299b0`, 7 `0x141794410`, 8 `0x1409f1b50`, 9 `0x1409f1640`, 10 `0x1409224d0`, 11 `0x141794410`.
- Wrapper `0x140925750` (26 insns, capstone): `inc [rcx+0x10]` (frame counter) → `call 0x1408bf090` → **`call 0x140925470` unconditional** → `%25 gate` (`mul 0x51eb851f`, `cmp r8d,[rbx+0x18]`, `sete`, `call 0x1408bfcf0`) → `call [rax+0x40]` (slot 8 = `0x1409f1b50`).
- The `counter % 25 == index` flag goes only to `0x1408bfcf0`, which never reads it (A4TeamTick correction stands). **Direct E8 callers of `0x140925750`: none** (whole-code scan). It is reached only by virtual dispatch (`call [rax+0x10]`).
- Capture check (CONFIRMED): 14/22 Player objects captured (indices 8–21 from `[obj+0x18]` dword); `[obj+0x10]` = `0x771a` = 30490 = `MI+0x1198` frame in every one (counter = frame, increments once per tick). Indices 0–7 live in uncaptured pages.
- Owner array (CONFIRMED): match object M (`0x1f230710`, vtable `0x142596dc8`) holds `M+0x10[0..21]` = 22 Player pointers (first 8 uncaptured `0x37de…`, last 14 as above). Matches the setup store `[rdi+rcx*8+0x10]` with `rcx = side*0xb+slot` in `FUN_0x1404187b0` (loop `r8d` 0..1 × `edx` 0..10, `imul rcx,rax,0xb`, `call 0x1409211e0` factory at `0x14041885f`).

### 1.2 Driver arguments (CONFIRMED, `0x140925470` full disasm)

- Entry: `rbx = rcx` (Player self). Everything else is reloaded from globals; **the hub msg/dt args are discarded** (`xor edx,edx` at `+0x4a0`; `rsi` reloaded `[G+0x1a8]`; `rdi` overwritten by `0x140922ff0` return). The driver's only input is **self + G/MI globals**.
- Team→player handoff, first thing per player per frame (CONFIRMED): `eax = [rbx+0x18]` (global index 0–21) → **side fold** (`cmp 0xb; setl dl` path: `edx = (eax>=0xb)`) → `call 0x140a48a30(G, edx)` = `T_side`. So `T_s` is fetched fresh for every player (not passed down).
- Then: `r15 = MI = [G+0x270]`; `+0x1a488` sub-object via `0x140922ff0`; `[rdx+0x3ad8]` store; gate `0x140a606c0`; `MI+0x1314` check; `0x140a60550` checks with index; tails `0x14092fad0` + `0x14092af10` (both self-only + stack record).
- Wrapper signature `(self, msg, dt)` matches the hub listener slot-2 shape exactly (`movss [rsp+0x18],xmm2` prologue, same as H slot 2). **The per-player tick is a hub record delivery, one record per player per frame.**

### 1.3 Order over the players + the divisor (CONFIRMED registration; INF delivery)

- Registrar `FUN_0x140418940` (M.slot1 `0x14041aae0` state machine region) runs a 22-iteration loop (`sil` 0..10 inside `rbx` 0/0xb, `cmp sil,0xb` + `cmp rbx,0x16`): per player `eax = counter++`, **`div eax, [0x1434456B8]`** (`F7 B5 7D 2D 03 30` at `0x140418a35`); if remainder == 0 → `FUN_0x140418710(r13, edi)` creates a listener + hub Register at type index `edi`, and `FUN_0x140419a10(r13, edi+1)` registers a second; `edi += 2`.
- So each registered player gets its **own record type index** (or a pair). Per-frame delivery order = ascending type index = **global player order 0→21** (for players with `counter % divisor == 0`). CONFIRMED code shape; the divisor at `0x1434456B8` is a **runtime `.data` word (0 in the static image — set at runtime)** and the type-index base `edi = [M+0x100]+1` is runtime. **OPEN remainder (named): read `0x1434456B8` + `[M+0x100]` live once to close side-1 gating and the exact base.**
- Per-player listener names (CONFIRMED strings): `"PlayerListener"` (`0x142593c48`, used at `0x140418a96`, len `0xe` via `0x141b501c0`), `"post_collision"` (`0x1425951b8`, the paired `419a10` listener), `"RefereeListener"` (`0x142593c58`, `0x140418bda`), `"MatchMainListener"`/`"MatchSubListener"` (GapsA2), `"match_begin"`/`"match_end"`/`"analyze"` (analyze registrar `0x140419250`, two `vt+0x30` calls at `0x1404192df/33d`).
- Sibling creators `0x140418640/450/5a0` + `0x140419a10` all share the shape (alloc → ctor `0x14042ff10/27320/2f600` → `0x140432cd0(index)` → hub `+0x30` → `0x141b501c0` name wire). Only `0x140418710` creates `MatchObjectListener` (`0x14042fe20`); it is the shared per-side/per-player listener factory.
- **S8 stage consequence: PORT the chain now.** Driver takes self-only; handoff fields (`T+0xa2a0/28c/278/26c/78/255/254` reads, `pl+0x5d0`/`AI+0x8d4`/`P+0xc6bc` writes per A4TeamTick §5.2) flow through the already-mapped accessors. The only runtime unknowns (divisor value, type base) affect *which* players tick, not the chain body. The old "dispatcher unknown" FLAG is replaced by: hub record delivery at per-player type indices, order 0→21, gate `counter % [0x1434456B8] == 0`.

## 2. S9: hub Register, referee, animation, camera — CLOSED

### 2.1 Register `vt+0x30` family (CONFIRMED, was the top OPEN)

Hub vtables `0x142ac1cb0/1d48/1de0` share slots; the Register arms are:
- `vt+0x30` = `0x141b3db70`: `[hub+t*8+0x20] = listener` (plain overwrite; old released via `[rax+8]`). `r8==0` path builds a default.
- `vt+0x38` = `0x141b3db40`: `[hub+t*8+0x1820] = listener` (the "always" table).
- `vt+0x40` = `0x141b3dc50`: `[hub+t*8+0x1020] = listener` (channel-0 table).
- `vt+0x48` = `0x141b3dc90`: `[hub+t*8+0x820] = listener` (channel-2 table).
- The match registrar region (`0x140418000–0x14041a000`) calls **only slot `0x30`** (39 `ff 50 30` sites, zero `0x38/0x40/0x48`): every match listener registers into the **`+0x20` table at its type index `t`**. The channel/`+0x1820` tables are untouched by the match path.

### 2.2 Hub dispatch order (CONFIRMED, `0x141b3edd0`)

Per record: `low7` = type, `bits7–16` = channel. Type 1 = pointer-follow; type 3 = end. Else: channel 0 → `[hub+0x1020+t*8]`; channel 2 → `[hub+0x820+t*8]`; else → `[hub+0x20+t*8]`; each `call [rax+0x10]`; then **always** `[hub+0x1820+t*8]`, `call [rax+0x10]`, next record. **Order for a type-2 record: the `+0x20` holder first, the `+0x1820` holder second — no null checks** (so the `+0x1820` slot is non-null at runtime via hub-ctor defaults or the outer pump hub; the match hub object `M+0x10 = 0x37de64c0` was not captured — named remainder, does not block porting).
- The pump: slot 6 `0x141b3dd80` → `0x141b3ea30` drains one queued list per call (GapsA2 2-frame lag stands). **Per-frame order across subsystems = record queue order (producer order), not registration order.** Within one type: the two holders above.

### 2.3 Referee (CONFIRMED chain to slots; event-driven, not ticked)

- Referee vtable bytes verified in image at `0x142598558`: slot 0 `0x140422030`, slot 2 `0x140420f8c`, slot 3 `0x140423000` (the `+0x8` qword is data, not code — earlier "+0x8 slot" readings were off by one; the code slots are 0/2/3).
- Chain CONFIRMED end to end: slot 0 `0x140422030` → … → `0x140445e90` → `0x14044c880` (call at `0x14044629c`) → outer record switch `0x14044c7b0` (codes `0x3b–0x42`, `add edx,-0x3b; cmp 7`) → judge switch `0x14044a180` (codes `0x29–0x31`, `add edx,-0x29; cmp 8`, 9-arm jump table) → judge fns → whistle `0x14050e770` (5 E8 callers CONFIRMED: `0x14050cba2/ccbc/e0f5/e2f2/e47d`, all judge-side).
- Slot 0 body contains the referee→restart linkage: `0x140422888` (inside `0x140422030..0x140422a26`) calls `0x140a3cae0`, the **MI+0x1775 mode writer** (§4). Slot 3 `0x140423000` → `0x1404514f0` (event-driven, record `0x23`/`0x4` + `MI+0x1554==4` arm per GapsA2).
- The referee object itself is heap-built (zero image-wide holders of the vtable; no `lea` to it in code range) — construction site stays OPEN, placement does not need it.
- **RefereeListener exists** (`0x142593c58`): the referee IS hub-driven via its own record type (runtime index), ordered by queue position. No tick-caller exists because none is needed.

### 2.4 Animation + camera (CONFIRMED render-side; MA-build-then-animate order stands)

- `InplayCamera::update` (`0x14087f280`): **zero E8 callers image-wide** (CONFIRMED) — virtual/render path only, consistent with camera-target doc (per-rendered-frame cadence, 54 Hz reference). Same applies to pose application: no per-tick caller in the match path.
- MA consumers (`0x140530320/532f70/587100/5b89f0/5e6e80`) run inside the team tick ( MA chain under `0x1404f6c30`); pose/camera consume the built MA **after** the tick, on the render side. **MA-build-then-animate order: CONFIRMED** as tick-then-render (not tick-then-tick).
- Restart sequencer: whistle (judge path above) → `MI+0x1775` writer `0x140a3cae0` from referee slot 0 → mode-gated team-tick arms (`MI+0x1775 == 0x17` restart branch in step 20–23). **Wire order: whistle → 1775 → restart branch. CONFIRMED.**

## 3. S4: MA nearest-list writer — NARROWED, still OPEN

- The builder writes `MA+0x22e4c + 200·i` with **no literal displacement** (CONFIRMED: 275 raw `0x22e4c` hits collapse to 49 true memory ops after add-immediate filtering, all `lea`/loads/consumers; zero stores). A computed base (`imul 0xc8`-family) is required.
- Excluded as the list builder (CONFIRMED by d2-operand reads — all are **ball-relative** `d²=(x−MA+0x2482c)²+(z−MA+0x24834)²`, i.e. carrier/cluster, not player-vs-player): `0x1405375a4` + `0x140536bdd` (in `0x140536fc0`/`0x140536690`), `0x140535b55` (in `0x140535a10`), `0x1406361c0/1e8` (in `0x140635ab0`, table reads `[rax+r15*4+0x3f04/0x3f0c]` — different subsystem).
- Candidate profiling (float stores, 22-loop): `0x1404567e0` (29 insns, no), `0x140439ef0` (24, no), `0x14043a0a0` (161, no fstores), `0x140639a20/830` (555/674 insns, 3 fstores each, no `cmp 0x16`), `0x140a42b80/940` (7/54, run-state only). **None qualifies; the builder is not among the MA-chain callees profiled.**
- Sort-key recompute check stands (A4TeamTick §3.2: stored d² matches recompute outside rebuilds). Cadence stays INF(h): once per frame inside the team-tick window.
- **OPEN remainder (narrowed): the 200-stride loop over `i = 0..21` storing `{id, d²}` + counts (`+0xb0/b1`) + ranks (`+0xb2`, absent = 10). Next step: decompile the two unprofiled large MA-chain callees' parents (`0x140534a20` len `0x763` has `cmp 0x16` at `0x14053528b` — its enclosing loop needs the d2-operand check) or trace the `+0xb2` rank consumer `0x140538050` backwards. The `// ponytail:` writer-unknown marker for S4 stays.**

## 4. S3: MI phase-word writers — CLOSED per word

Store scan (memory ops only, add/cmp/lea false positives removed) + nearest-head attribution + role/callers. Event-store lookalikes (`param_2 = E+0x180c60`, GapsA2 b3/c) are **not** MI writers and are called out per word. Live capture values (frame 30490, open play) confirm widths: `11b4` byte 0 = 0 (gate open), `1775` byte 0 = 0, `1748` bytes = 1 (first slots on-pitch).

| Word | Width | Writer(s) + role | Caller(s) | Live |
|---|---|---|---|---|
| `+0x1168` | dword (byte store at `1411821be` is a sub-field clear) | `0x1408ad0b0` INIT (with `11a0=0xa`) ← `0x1408acc09`; `0x141180540` RESET (zeros) ← 7 calls (`141174d75/18d853/196339/1b74fd/1b757a/1ba75a/214175a`); `0x141183710` second reset ← `0x1411fc3ef`; `0x140bc8450`, `0x141cc3cd0` presentation (no E8 callers = virtual/engine-driven) | init/reset/presentation | 3 |
| `+0x116c` | **byte** | **sole writer `0x141180540`** (`mov [rbx+0x116c],0` at `0x1411821c5`) — the same RESET fn | as above | 1 |
| `+0x1554` | dword (`140a3c68a`) + byte (`141190fe9`) | `0x140a3c560` ← `0x140a3bcce`; `0x141190f10` ← `0x141116a44` | team-tick-side + presentation | 0 |
| `+0x11b4` | **byte gate** (`==0` open); dword stores are width artefacts | INIT `0x1408ad0b0`; RESET `0x141180540` (`=0x83c60000` full-word pattern); event-store lookalikes in `0x140a107c0` (`mov/inc [rdi+0x11b4]`, param_2 = E) — **not MI** | init/reset | byte0 = 0 (open) |
| `+0x1198` frame | dword | INIT `0x1408ad0b0`; COPY `0x14100cca0` (`mov [rbx+0x1198],eax` at `0x14100ce6c` + `119c/11a0` adjacent — the whole-struct refresh, GapsA2 b3 CONFIRMED shape) ← 7 calls; RESET-byte forms in `0x1410d9080/14117e340`-family; event-store `inc` in `0x140a115a0` (`0x140a128e4`, E-side) — **not MI** | copy callers `1410d97a6/9942/111025c/13b2f8/13b427/13b5d7/14e17b2` | **30490 = frame** |
| `+0x11a0` (E5) | dword (+ byte sub-forms) | INIT `=0xa` (`0x1408ae72b`); COPY `0x14100cca0` (`0x14100ce84`); per-tick `+1` is the **event-store** `inc [rsi+0x11a0]` at `0x140a127b5` (E-side) copied over — no direct MI per-tick store exists (GapsA2 b3 stands) | — | 2325 (≠ frame: copy lags/leads; GapsA2 b2 lockstep was window-local — **corrected**: not a universal lockstep) |
| `+0x1775` mode | **byte** | `0x140a3cae0` (`mov [rcx+0x1775],dl`) ← **`0x140422888` = referee slot-0 path** + `0x141fcf805/852`; `0x140413ce0` (`mov [rbx+0x1775],al`) ← `0x1404141c7/422860/4433f7` | referee-driven | byte0 = 0 |
| `+0x1748[22]` on-pitch | bytes | `0x1406d8f50` ← `0x1406a9ec0`; `0x141e5ed10` ← `0x141e5f464`; `0x1422bd460` ← `0x1422bd0ab/3c0`; `0x140a3cae0`-family (`0x140a3ccf0` in same fn) | mixed engine/match | `0x1010101` first 4 = 1 |

E5/note correction: `MI+0x11a0 = 2325`, `MI+0x119c = 1474` at frame 30490 — the "Δ11a0 = Δframe every tick" finding (GapsA2 b2) held in its window but is **not** a universal identity; S3's struct must treat `1198/119c/11a0` as copy-refreshed (writer `0x14100cca0`), not as frame aliases.

## 5. Stage table update (plan §4)

- **S0** — unchanged (needs live capture; RegCapture owns it). Not touched.
- **S1** — unchanged. Accessors `140A48910/48A10/489F0/489D0/48A30` re-confirmed as the G-table path (driver + chain call them per player per frame: `140a48a30` in driver head, `140a48910/48a10/489f0` throughout `0x140928fc0/9257b0/92c9f0`). No new blocker.
- **S2** — unchanged (needs block allocator = OPEN-1, still open). Player-object side notes added: object ≥ `0x1a5e0` bytes (factory `0x1403fb410` size at `0x140921205`), index at `+0x18`, counter at `+0x10`.
- **S3** — **UNBLOCKED.** All 8 words have writers + roles + callers (§4). Stand-ins: `IsInPlay` ← `116c/1775`, stoppage arms ← `1554/1775`, frame ← `1198`, E5 ← `11a0`-via-copy, on-pitch ← `1748`. `// ponytail:` per word names the writer VA.
- **S4** — **still BLOCKED, narrowed.** Ships as known-approx writer with the §3 exclusion list; the exact builder is the `+0xb2`-rank/200-stride loop (next step named in §3).
- **S5** — unchanged (needs slices; `1404FEDC0` unmodellable split stands).
- **S6** — unchanged. Note: `140A40820`-family diffed set stands; undiffed list unchanged.
- **S7** — FLAG narrows: H skeleton order CONFIRMED (§1.1 re-verified); remaining FLAG items are the unmodellable pair + unattempted `14041ECF0`-region fns (H.b/c/d decoded here by disasm — `MI+0x1168==10` skip at `0x14041ed07`, T-bind, tick loop, planner, teardown — but not diffed).
- **S8** — **UNBLOCKED (chain shippable).** §1: wrapper + driver + full downstream call lists (driver→`14092fad0/92af10/926890/6b00`→…→`140928fc0`→`1409257b0`→`140929a00`→`14092a340`→`14092c9f0`→`1421019d0`→`142110d30`); args = self-only; team fetched per player via `140a48a30`; order = global index 0→21 over per-player record types, gate `counter % [0x1434456B8]`. Remaining named unknowns (divisor value, type base `edi`, hub `+0x1820` default) do not block porting the chain. Registrar `0x140418940` + factory `0x1404187b0` + names give the setup side.
- **S9** — **UNBLOCKED (placement shippable).** Register arms + dispatch order CONFIRMED (§2.1–2.2); referee chain to slots CONFIRMED event-driven (§2.3); whistle→1775→restart order CONFIRMED; animation/camera render-side CONFIRMED (zero E8 to `0x14087f280`). GF `Match::Process` reorder justification: team tick (type-2 `+0x20` holder path) runs inside the pump's per-record dispatch; referee/players run on their own record types in queue order; animation/camera run after, off-tick. Remaining named unknowns: match-hub `+0x1820` default contents (uncaptured object `0x37de64c0`), referee-object ctor, exact per-frame queue order (producer-side, needs a live multi-record capture).

## 6. Open items (ranked, updated)

1. ~~Hub Register `vt+0x30`~~ — CLOSED (§2.1). Remainder: `+0x1820` default (needs hub object `0x37de64c0` bytes — one live read when PES is up).
2. ~~`match::Player` slot-2 loop caller~~ — CLOSED as hub record delivery (§1.3). Remainder: divisor `[0x1434456B8]` value + type base (one live read each).
3. Per-tick MI/roster binder set/clear — narrowed: MI reads are param-carried; the copy `0x14100cca0` + 7 callers is the refresh path. Remainder: which caller runs per tick (needs frame-trace coverage, S0).
4. `MA+0x22e4c` writer — OPEN, narrowed (§3).
5. `T+0x78` writer — was OPEN in plan §5; RL §4 already closed it (`FUN_1404FB380 := F0E50(...)`; lane leaves `4FB2A0→4FB380→F0E50`). No action.
6. Block allocator (OPEN-1) — untouched this pass.
7. Unmodellable `1404F6C30/1404FEDC0` — untouched (translation pipeline owns).
8. Alloc sizes / `+0x1a/1c` shape — untouched (PR owns).
9. ~~Animation/camera per-tick callers~~ — CLOSED as render-side (§2.4).

## 7. Method note (for the next agent)

Ghidra bridge `:8080` refused connections all session (the `xd://mcp__ghidra_*` routes fail the same way). Everything above is capstone-over-image (`offset = VA − 0x140000000`, verified at 10 sites) with `mmap` reads — **do not `read()` the 462 MB image into eval memory** (the 2 GB `ulimit` kills the cell; `mmap` + chunked scans work). Whole-code E8 scans take ~5–50 s each; chunk at 32 MB. `head()` = nearest `CC CC` + push-byte heuristic; verify against known neighbours (the `0x140a0e6d0` overshoot in §4 is the failure mode). Capture `cap1` regions are VA-named (`base-end.bin`); `g.bin` is the `0x2E58` G blob; `live.json` has MI/G/H/M/MA/teams VAs.
