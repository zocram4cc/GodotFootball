# GapsA2: tick placement, HUD clock, E5 writer, C8

Agent: GapsA2. Read-only. Static: Ghidra bridge (`:8080`, decompile + `get_xrefs_to`), PE image bytes. Live: PES2021.exe pid 2510857, `pread` on `/proc/<pid>/mem` through `sudo -n` (the process is not dumpable, so the user alone is refused). No writes, no pins. Scratch: `the scratch dir` (`live_series.py`, `series_a.txt`, `scan_disp.py`, `hits_*.txt`, `d_*.c`).

Tags: **OBS** = seen in decompile, disassembly, xref or live read. **INF(h/m/l)** = inference. **OPEN** = not closed.

## 0. Verdict table

| # | Question | Verdict | Confidence |
|---|---|---|---|
| a1 | MA build position | **Inside the team tick**, not ahead of it. GapsMatch §5 / §9.6 is wrong on this. | OBS |
| a2 | Team-tick delivery path | Closed: MatchObjectListener slot 2 → `FUN_1404334e0` → gate → H slot 2 → `FUN_1404f6c30`. | OBS |
| a3 | Referee position | **Not in the tick.** Event-driven from a record switch. | OBS (callers), INF on the switch's own caller |
| a4 | Listener vs MatchSub order | **OPEN.** The hub Register body (`vt+0x30`) is undecoded. GapsMatch's "match listener = FUN_14042b9f0" label is wrong. | OBS on identities, OPEN on order |
| b1 | HUD clock source: E at `G+0x2c0` | **Refuted** as HUD source (live + static branch). The true source remains OPEN. | OBS |
| b2 | E5 step | Frame delta: Δ`MI+0x11a0` = Δ`MI+0x1198` = Δ`MI+0x119c` on every sampled tick, 53.8 Hz. | OBS |
| b3 | E5 writer | Kind-27 accumulator is in the event store (correction 3a **confirmed**). The per-tick **MI** writer is **OPEN**; the best candidate is a whole-struct copy. | OBS (static), INF med (copy) |
| c | C8 (E7 `MI+0x11b4`) | Width error in the live note. The gate is a **byte** at `MI+0x11b4`, polarity `== 0` (open). The byte is 0 in every live sample, so the gate is inert in observed play and stoppage. | OBS |

## a. GapsMatch #5: listener order and the MA build

### a1. MA build position (OBS, verified)

- `FUN_140536fc0` (MA build body), single caller `FUN_140535c60` at `0x140536002` (xref, UNCONDITIONAL_CALL).
- `FUN_140535c60` (master match tick), single caller `FUN_1404f6c30` at `0x1404f7955` (xref). The call is in the **team tick** body (`FUN_1404f6c30`, body `1404f6c30`…), not in a match-master function.
- `FUN_140535c60` is called with `param_1+0x4200`, a record `local_360`, `*(param_1+0x6d50)` (match object) and `uVar5`. The MA-build call `0x140536002` sits in the body's gated region; the decompile elides those lines, so the exact predicate is taken from GapsMatch §5 (`MI+0x116c==2 && MI+0x11d4==0`, `uVar3==0`) and is INF on the exact guard. The call site, not the guard, is what changes here.

Order inside `FUN_1404f6c30` around the call (decompile, call VAs):

```
… FUN_140501160 (1404f7800) → FUN_14057bbb0 (1404f7824) → FUN_14053b110 (1404f7843)
  → FUN_1404fb2a0 (1404f784b) → FUN_14057b6b0 → FUN_14057c190 → FUN_1404f8c70 → FUN_140500a00
  → FUN_1404fad30 (1404f7876)
  → [if local_398==0] FUN_140525530 run manager (1404f78ba, cond. on MI+0x1198&1 == param_1+8); FUN_140538050 (1404f78d4)
  → FUN_140a40820 ×11 loop (1404f78fe)
  → FUN_140535c60  = MA build chain  (call at 1404f7955)
  → FUN_140ca1a50 (1404f7961) → [cond] FUN_1404f88d0 → FUN_1404fc720 → FUN_140509170 → FUN_1404fc4d0
  → FUN_14057ba50 → FUN_14057d240 → FUN_140580630 → [if cVar4==0] FUN_140539ed0 → FUN_140421f40(0/1)
```

So the MA build sits in the middle of the team tick, after the run manager and the per-player counter loop, before the press/shape writers. The team tick runs once per side from the H routine (a4).

### a2. Team-tick delivery (OBS, verified)

Chain, with each link checked:

1. `FUN_140433760` is slot 2 (`+0x10`) of vtable `0x14259b1a8` (MatchObjectListener, ctor `FUN_14042fe20` sets it). DATA xref `0x14259b1b8`. Calls `this->vtbl[+0x20]` = `FUN_1404334e0`.
2. `FUN_1404334e0`: gate `(this+0x28)->vtbl[+8]` = handle slot 1 = `0x140426c10`, which returns the global byte `DAT_1436f09c2` (VA `0x1436f09c2`). If the gate is 0 → `FUN_140a8f0f0` (no team tick). If 1 → `FUN_140a8eff0(this+0x30, msg, dt)`.
3. `FUN_140a8eff0` walks the vector at `this+0x30` and calls `vtbl[2]` of each member. The member is H, vtable `0x142597580`; slot 2 = `0x14041ecf0` (DATA xref `0x142597590`). `0x14041ecf0` is not a Ghidra function entry; its bytes (`push r14/r15; lea rcx,[rcx+0x488]`) run a loop that calls `FUN_1404f6c30` at `0x14041edd7` (xref, UNCONDITIONAL_CALL; loop bound `cmp [r14+0x460], r12`, INF = side count).
4. `DAT_1436f09c2` is written by `FUN_140426c60` (`mov byte [0x1436f09c2],1` on entry, cleared on the return paths; it depends on `PTR_1434a2d50` vt+0x18 and `PTR_1434a2d40` vt+0xb8). Caller of `FUN_140426c60` not traced.
5. The gate is per-listener-object only through the handle's slot 1, which reads a global. So every MatchObjectListener (MatchMain and MatchSub, both built by `FUN_14042fe20`) reaches the team tick when the global is 1. The per-listener distinction is not in this gate.

### a3. Referee placement (OBS on callers; INF on the switch's caller)

Direct calls from the tick bodies to the referee: **none**. Checked bodies: `FUN_1404f6c30` (team tick), `FUN_140535c60` (MA chain), `FUN_1404267a0`, `FUN_1404267f0` (→`FUN_140a0d3f0`), `FUN_140426860` (→`FUN_140a0d520`), `FUN_140426760` (→`FUN_140a0d190`, `FUN_1404582c0`), `FUN_140426810`, `FUN_140433760`, `FUN_1404334e0`. None calls `FUN_14050b9b0`, `FUN_14050ee40`, `FUN_14050e770`, `FUN_14050c3e0`, `FUN_14050b490`, `FUN_1405b*`.

Referee entry points and their roots (xref chains, all UNCONDITIONAL_CALL):

- Time-up gate `FUN_14050b9b0` ← `FUN_14050c3e0` (14050c425) ← {`FUN_1404a56c0` (1404a584d), `FUN_1404494e0` (1404495f8), `FUN_140454570` (1404549c2)}.
  - `FUN_1404a56c0` ← `FUN_140449130` (1404491a3).
  - `FUN_1404494e0` ← `FUN_14044c880` (14044cc97).
  - `FUN_140454570` ← `FUN_14044c7b0` (14044c82a); `FUN_14044c7b0` ← `FUN_14044c880` (14044cbaf).
  - `FUN_14044c880` ← `FUN_140445e90` (14044629c); `FUN_140445e90` ← `FUN_140422030` (14042225e).
  - `FUN_140422030` = DATA slot 0 of vtable `0x142598558` (xref `142598558`).
- Judge `FUN_14050ee40` ← `FUN_1405b8000` (1405b8282), `FUN_1405b6660` (1405b67d7), `FUN_1405b83b0` (1405b88a8/8982).
  - `FUN_1405b8000` ← `FUN_1405b7470` (1405b7f48) ← `FUN_14050e4a0` (14050e58d) ← `FUN_1404514f0` (140451a8e) ← `FUN_140423000` (140423014).
  - `FUN_140423000` = DATA slot 3 of vtable `0x142598558` (xref `142598570`). Its body: calls `FUN_1404514f0(this+0x28)`; if record type is `0x23` or `4` and `MI+0x1554==4`, sets `this+0x30=1`. **Event-driven.**
  - `FUN_1405b6660` ← `FUN_14050df20` (14050df8e) ← `FUN_14050b490` (14050b4cf).
  - `FUN_14050b490` ← `FUN_14050c3e0` (14050c41d) and `FUN_14050cce0` (14050cd2e).
  - `FUN_14050cce0` ← `FUN_1404a42e0` (1404a4c4c) ← `FUN_1404a4200` (1404a42b4) ← `FUN_14044a180` (14044a1fe).
  - `FUN_14044a180` is a switch on the record code (`case 0x29`…`0x2c` shown; OBS). Its caller is **not traced** (OPEN).
- Whistle `FUN_14050e770`: GapsMatch's judge-side list stands (no new evidence).

Verdict (a3): the referee is reached only through record handlers (the vtable-slot-0/3 handler of `0x142598558` and the record switch `FUN_14044a180`). It is **not** in the team tick, the MA chain, or the listener steps 2–6. Its ordering against the team tick therefore depends on the hub's per-record dispatch (a4), not on a call inside the tick. A2 should not place the referee inside `Match::Process`'s tick steps.

### a4. Listener vs MatchSub order (OPEN)

Established (OBS):

- Two listener classes receive type-2 records:
  - **MatchObjectListener**, vtable `0x14259b1a8`, slot 2 = `FUN_140433760`. Both MatchMain and MatchSub are built by `FUN_14042fe20` in `FUN_140418940`: MatchMain block (`FUN_14042fe20` call at `0x140418754`, name `s_MatchMainListener_142593ce8`), then MatchSub block (`0x140418d1c`, name `s_MatchSubListener_142594778`). Code order in the registrar: MatchMain first, index `base`, MatchSub index `base+1`.
  - **Analyze listener**, vtable `0x14259a350`, slot 2 = `FUN_14042b9f0` (the record switch with steps 2–6; PE vtable read). Its ctor `FUN_14042b860` is called only from `FUN_140419250` at `0x140419578`. `FUN_140419250` registers the `analyze` channel (name `s_analyze_142593a38`, index `iVar1+0x8d`, `iVar1=*(this+0x100)`). `FUN_140419250` is called from `FUN_141fe8d10` at `0x141fe8d9c`.
- So **`FUN_14042b9f0` is not the match listener that carries the team tick**. The label "match listener (FUN_14042b9f0, ten steps)" in `pes-re-tick-order.md` §2 and GapsMatch §5 is wrong. Its own record-2 path (`FUN_1404267a0`, …) does not reach the team tick, by the direct-call check in a3.
- The hub dispatcher `FUN_141b3edd0` calls **one** object per slot, not a list: `*(hub+0x1020+t*8)` (subtype 0), `*(hub+0x820+t*8)` (subtype 2), `*(hub+0x20+t*8)` (other), then `*(hub+0x1820+t*8)` always. GapsMatch's "three arrays of listeners" is a single-pointer table. Which listener sits in which slot is set by the hub's Register at `vt+0x30`, which is **not decoded**.
- `FUN_140a8f730` is `push_back` (OBS), so vectors are appended in call order. The team tick vector (`this+0x30` in `FUN_1404334e0`) is filled by `FUN_140a8f460` (`H` push, from the `FUN_14041bcf0` thunk).

OPEN: the relative order of MatchMain/MatchSub (type 2) against the analyze listener (type 2), and whether the team tick runs before or after the analyze-listener steps. The decider is `hub vt+0x30` (its body) plus the call order of `FUN_141fe8d10` (`FUN_140419250` vs `FUN_140418940`). Neither is decoded. Next: decode the hub class's `vt+0x30` (the hub is `*(match+0x10)`; `call [reg+0x30]` at `0x1404186f4` and `0x140418796`), then check `FUN_141fe8d10`'s caller order.

## b. HUD clock

### b1. The env at `G+0x2c0` is not the HUD's env (OBS, refuted)

`FUN_1420efec0` (carried from `pes-re-clock-record.md` §2.2, OBS, not re-decoded here) reads `E = param_1[7]` (`[Time+0x38]`). If `byte[E+0xa4]==0` it uses `E+0xa0`; otherwise it uses `E+0x11c`. The live values (20 s window, 467 samples, `live_series.py`) are:

- `G+0x2c0` = `E`, `E+0x58` = 10.0, `E+0xa0` = 0.0, `E+0x11c` = 0.0 in every sample, including the `116c 1→2` stoppage at 19.89 s.
- `E+0xa4` = 117440535 (low byte `0x17`), so the HUD branch is the `else`. It reads `E+0x11c` = 0.0 → 0 seconds, which does not match a running HUD clock.

Conclusion (OBS): the HUD's env object is **not** `*(G+0x2c0)`. This confirms clock-live §2 and refines it. The writer of `[Time+0x38]` is the open item (b1-open). Not found in this session.

Note the `E+0xa4` low byte. The earlier notes said `0x28`/`0x2b`. Live now reads `0x17`, which is state dependent. Not an issue for the verdict, since both values take the `else` branch.

### b2. E5 per-frame step (OBS, verified)

From the 20 s series (`series_a.txt`), 265 consecutive play-phase pairs (`116c==1`), summed:

| Quantity | Σ Δ | Δ per second |
|---|---|---|
| `MI+0x1198` (frame) | 581 | 53.8 |
| `MI+0x11a0` (E5) | 581 | 53.8 |
| `MI+0x119c` | 581 | 53.8 |

Per pair, Δ`11a0` = Δ`1198` = Δ`119c` exactly (sample-by-sample, not only in sum). E5 step = frame delta (1 per tick). This reconfirms "E5 step = frame delta" (foundation §0.3d). Confidence OBS.

Note: `pes-re-clock-live.md` §3/§4 has a "random-walk 977→635 (NOT a counter)" finding for match 4. In this window `11a0` is monotone with the frame. The two observations are not reconciled (different windows and possibly a different possession state). OPEN, low priority.

### b3. E5 writer (static; partly OPEN)

Every direct store to a dword at `+0x11a0` in the image (disp scan of `objdump -d` Intel listing, 61 hits; `hits_11a0.txt`):

- `1408ae72b` `mov [rbx+0x11a0],0xa` in `FUN_1408ad0b0` (initialiser region; INF it is the MI constructor): init to 10.
- `14118221c` `mov [rbx+0x11a0],edi` in `FUN_141180540` (zero reset, decompile line 1088 `*(param_1+0x11a0)=0`).
- `14100ce84` `mov [rbx+0x11a0],eax` inside `FUN_14100cca0` (whole-struct copy-assign; copies `+0x1198`, `+0x119c`, `+0x11a0` at `14100ce6c/ce78/ce84`).
- `141183362`, `141184546`: byte zero stores.
- `140a127b5` `inc [rsi+0x11a0]` in `FUN_140a115a0` kind-27 arm (`case 0x1b`), with `param_2` = **E+0x180c60** (call `FUN_140a13060` → `FUN_140a115a0(param_1, *(param_1+0xbd08)+0x180c60, …)` at `140a130b5`, decompile-verified). This is the event-store count at **E+0x181e00**, and the duration sum is `119c` → **E+0x181dfc**. This **confirms correction 3a** (foundation §0.3a), and the MI-side "kind-27 feed" (C6/C7) is withdrawn.

So no direct per-tick store into **MI+0x11a0** exists in the listing. INF(med): MI+0x1198/119c/11a0 are refreshed by a whole-struct copy (`FUN_14100cca0` writes exactly those three fields at `14100ce6c/ce78/ce84`) from an object whose `+0x11a0` is the event-store count. Support: the live lockstep (Δ=+1 per tick) matches a per-tick +1 in the event store. Not shown: that any caller of `FUN_14100cca0` uses MI as destination. Callers of `FUN_14100cca0` (xref): `FUN_1414e1790`, `FUN_1410d9080`, `FUN_1410d9890`, `FUN_1414e17d0`, `FUN_14100d250` (`14100d290`), `14111025c`, `FUN_14113b280`, `FUN_14113b520`. OPEN.

Verdict (b3): E5 = event-store kind-27 count (`E+0x181e00`, per tick, +1). MI+0x11a0 is its copy (INF med). **OPEN:** the MI-side copy site. Next: check which of the eight callers uses `MI` as destination; or decode `FUN_1410d9080` / `FUN_14113b280`.

## c. C8: E7 `MI+0x11b4`

- The gate is a **byte**. `FUN_140a17810` (K=10) and `FUN_140a178d0` (K=9) both load `MI = *(G+0x270)` and test `byte [MI+0x11b4] == 0` (OBS, decompile). Entering when the byte is 0 is the open predicate. PoseVeto §14's `== 0` polarity is what the code does.
- The live "0x100" is a **dword** read over `+0x11b4..+0x11b7`. Live bytes:
  - play: dword 16777472 = `0x01000100` → bytes `00 01 00 01`;
  - stoppage (`116c=2`): dword 256 = `0x00000100` → bytes `00 01 00 00`.
  - So `+0x11b4` (the gate byte) is **0 in all 467 samples**. `+0x11b5` is 1. `+0x11b7` flips 1→0 at the stoppage. The gate is open in both states.
- Writers into `MI+0x11b4` (static, 15 hits; `hits_11b4.txt`): `1408ae766` `mov [rbx+0x11b4],edi` (initialiser), `14118223d` `mov word [rbx+0x11b4],0` (reset), and `lea`-copies at `14118015f/166`, `1411890fb/141189102`, `14118b1eb/1f2`. No per-tick store to the MI gate byte. The `inc`/`cmp` writers at `140a10eb7` and `140a10e9c` and the `mov [rdi+0x11b4],r14d` writers at `140a1098e/10add/10de3` are in `FUN_140a107c0`, and their `param_2` is the **event store** (`E+0x180c60`, so their `+0x11b4` is `E+0x181e14`, a consecutive-tick counter). They are not MI.

Verdict (c): the conflict is a **width error in the live note**, not a polarity or identity error. The gate is byte `MI+0x11b4 == 0` → open. Live it is always open (byte 0 across play and stoppage). The per-tick closer for K=9/K=10 is not in MI+0x11b4 (no per-tick writer).

**Decision for the owner (C8):**
- Adopt the byte: GF's gate reads `uint8 MI+0x11b4`, predicate "== 0 ⇒ K=9/K=10 eligible". Keep `== 0`.
- Do **not** flip polarity; do **not** use the dword `0x100`.
- Consequence: the gate never closes in observed states. If the owner wants K=9/K=10 to ever be off, the closer must be found elsewhere (candidates: `E+0x181e14` consecutive-tick counter in the event store, if the detector reads it through a different alias; OPEN). Until then the gate is inert; the port can keep it for fidelity with a `// RE-` marker.
- Gate C4 (per foundation §5.3) can unblock on this answer.

## Claims not closed

1. Hub `vt+0x30` body and the listener slot order (a4). Next: decode the hub class's vtable slot 6 (`+0x30`) and the call order in `FUN_141fe8d10`.
2. Caller of `FUN_140426c60` (sets `DAT_1436f09c2`, the team-tick gate) (a2 step 4).
3. Caller of `FUN_14044a180` (the record switch that reaches the referee) (a3).
4. HUD env writer `[Time+0x38]` (b1). Next: find the Time object (`FUN_1420f0eb0` Time::update) constructor and its `+0x38` store.
5. MI-side copy site of E5 (b3). Next: the eight `FUN_14100cca0` callers; check which uses MI as destination.
6. The per-tick closer of K=9/K=10 (c).

## Corrections to existing docs (for the owner)

- GapsMatch §5 and §9.6: "FUN_1404f7955 (per-player call site, in the match master)" → it is inside the team tick `FUN_1404f6c30` (call `0x1404f7955`). "MA build runs ahead of the team tick" → the MA build runs **inside** the team tick, mid-body.
- GapsMatch §5/§9.6 and `pes-re-tick-order.md` §2: "match listener = FUN_14042b9f0" → that is the analyze-channel listener (vt `0x14259a350`). The team tick is delivered by MatchObjectListener slot 2 (`FUN_140433760`).
- GapsMatch §9.6 "three listener arrays": the dispatcher reads one pointer per slot (`FUN_141b3edd0`).
- Foundation §5.3 C6/C7 (kind-27 feeds MI): withdrawn by correction 3a, now re-confirmed (`E+0x181e00`/`E+0x181dfc`).
- Foundation C8: resolved as width, see §c.
- `pes-re-clock-live.md` §2: E+0xa4 low byte now reads `0x17` (state-dependent).

## Reproduction

```
# Live (read-only): sudo -n python3 live_series.py 20 series_a.txt   (needs sudo: process not dumpable)
# Static: objdump -d -M intel --no-show-raw-insn PES2021.exe > pes_text.asm   (306 MB; deleted after use)
#         python3 scan_disp.py 0x11a0 > hits_11a0.txt    (also 0x11b4, 0x1198, 0x119c)
# Decompile: curl -s "http://127.0.0.1:8080/decompile_function_by_address?address=0x…"
# Xrefs: xd://mcp__ghidra_get_xrefs_to {"address":"0x…"}
```

Checks for the main agent: none run (read-only assignment). The report contains no code changes.
