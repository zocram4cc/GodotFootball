# H, S and brain records for the S7 frame step (S7Records, 26-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`
(offset = VA − 0x140000000). No PES bytes quoted.
Labels: **CONFIRMED** = disassembly bytes or snapshot bytes read this pass;
**INF** = inference; **OPEN** = narrowed remainder with the exact next step.
Scratch: `<workdrive>/gf-S7Records-scratch/`
(`read.py`/`fsnap.py` chunked snapshot readers, `dasm.py`, `d_*.txt` disassembly dumps).

Snapshots checked (all open play, all mid-match): FullSnap
`snap1-142012` (frame 30381), `snap2-142339` (31953), `snap3-142654` (40548),
same G/MI/H/T addresses in all three, plus LiveState `live/` (pid 2876063,
frame 11758, different boot, different H/brain addresses, same T/MI addresses).
H globals: `*(0x1436F3F98)` = H; H vtable `0x142597580` slot 2 = `0x14041ECF0`.

Prior docs re-checked, not re-derived: `docs/re/pes-re-integration.md` §1/§4/§5
(order, record table, S0–S9), `docs/re/pes-re-integration-open.md` §1–§2 (S8/S9
closed: per-player hub delivery, hub Register arms, referee chain,
whistle→1775→restart), `docs/re/pes-re-teamtick.md` §2 (H.a–H.f, 48 tick steps),
`docs/re/pes-re-player-record.md` §1–§2 (per-player 0x3F40 block),
`docs/re/pes-re-record-layouts.md` §2–§4 (slot/man/anime fields, T+0x78),
`docs/re/pes-re-blocked-slices.md` T10c/T12b (H+0xBB8 row table, 48 concept rows).

## §0 Float/constant table (script-read from the image, never typed)

No new float constants were needed: this ticket decodes record *layout* (sizes,
offsets, widths, vtables, call order), not arithmetic. The one float the H step's
chain reads that a task agent must not retype is the 1.0 unit weight at
`0x14256AE1C` (already carried 1:1 as `kUnitValue` in `pesteamtick.hpp:33`).
No instruction cited below depends on a neighbouring constant.

## §1 H record (the H-step owner)

### 1.1 Size, ctor, dtor, owner, lifetime

| Item | Value | Evidence |
|---|---|---|
| Size | **0x9B8** | CONFIRMED: registrar `0x140418DBB mov edx,0x9B8` before `call 0x1403FB410`, H ctor `FUN_14041E4D0` called on the result at `0x140418DD5`; corroborated by H-dtor dealloc `0x14041ECC5 mov edx,0x9B8`. Furthest H field (side-1 T_cache qword H+0x9B0..0x9B8, §1.2) == alloc end. (The `0x6DF8` at `0x14041E62A` is the *brain* alloc — §3.1, not H.) |
| Ctor | `FUN_14041E4D0` | CONFIRMED: called once from `0x140418DD5` (`call 0x14041E4D0` — the only image-wide E8 caller, whole-code scan); stores match `+0x788` at `0x140418DE1` and the global `*(0x1436F3F98)` at `0x14041E87F` (`mov [rip+0x32d5712],r14`). |
| Dtor | slot 0 `0x14041EC80` → `0x14041E960` | CONFIRMED: H vtable `0x142597580` slot 0 = `0x14041EC80` (image qwords); it calls `0x14041E960`, which clears the global (`mov [rip+0x32d5610],rbp/0`), destroys `H+0x460` through its vtable, then the two `H+0x450` brains. |
| Owner | match registrar region `0x140418940` | CONFIRMED: `0x140418DD5` is inside the registrar's H-creation block (`0x140418CB1` alloc `edx=0x18` → `0x14041E180` → store `+0x780` → `0x14041E2C0` → alloc `edx=0x70` listener → H ctor → store `+0x788`). |
| Lifetime | per match | CONFIRMED: created in the registrar (match setup), destroyed by slot-0 dtor; the per-frame step never allocates or frees it (H.d–H.f only read/write fields). |

### 1.2 Field table (every field the H step's chain reads or writes)

Addresses are H-relative. Types/widths from opcode widths (dword = u32/s32,
byte = u8/bool, qword pointer). "Step" = which H sub-step touches it (§4).

| Offset | Width | Name (provisional where noted) | Step reads/writes (VA) | Snapshot (4 captures) |
|---|---|---|---|---|
| `+0x0` | qword vtable | H vtable (`0x142597580`) | H.b–H.f run through it (slot 2 = step fns) | same in all 4 |
| `+0x10` | byte | (H ctor clears to 0 at `0x14041E871`; S-fn `140423570` path) | ctor write; H.f teardown path reads | 0 live |
| `+0x18` | inline obj | H+0x18 sub-object (teardown target) | H.f `FUN_1405069A0(H+0x18)` (`0x14041EF06`); `FUN_140505480` clears `+0x28/+0x2c/+0x8c` | — |
| `+0x450` | qword ptr | **brain[0]** (side 0) | ctor `0x14041E657` stores alloc; dtor clears; H.d `0x14041EDC0` loads, null-tested, arg1 of `1404F6C30` | non-null all 4 (addrs differ live vs snaps) |
| `+0x458` | qword ptr | **brain[1]** (side 1) | same as +0x450, second loop iter | non-null all 4 |
| `+0x460` | qword ptr | **planner object** (side planner / ball-phase writer) | H.e `0x14041EDC0`…`0x14041EE57` null-tests; arg1 of `FUN_1404405D0` | non-null all 4 (`0x2c6a89d0` snaps, `0x33124ba0` live) |
| `+0x488` | inline obj 0x4B8 | **S** (per-frame shared record, §2) | S-init `0x14041ED02`, tick arg2, planner arg2, teardown `0x14041EF1D` | §2 |
| `+0x910` | 0x28 bytes flags | **S-init flag block** (40 entries; S-init reads `[S+0x488+k]`, i.e. H+0x910+k) | S-init `0x140A49C90` reads per k=0..0x27; all set (0x01) in all 4 captures = per-match init done, per-frame calls return immediately | all 0x01 × 0x28 |
| `+0x940` | inline obj | H+0x940 sub-object (vtable `0x142597558`) | H.e binds `+0x10→+0x1150` chain once (`0x14041EE27-40`), passes `+0x20` slot; teardown loop `0x14041EE9D` | vtable same; `+0x10` = `0x0A3EEB60` all 4 |
| `+0x948` | byte | H+0x940 bind-once flag | `0x14041EDFA` tests, `0x14041EE23` sets | 1 all 4 |
| `+0x950` | qword ptr | H+0x940 child (`0x0A3EEB60`, vtable `0x1425974B8`) | read `0x14041EE2D` (`[rsi+0x10]`) | same addr all 4 (image-persistent object) |
| `+0x958/0x960` | qwords | H+0x940 spare slots | zeroed at ctor; teardown xchg path | 0 all 4 |
| `+0x970` | 0x28 side-0 bind block | byte flag +0x0, owner ptr +0x8, **T_cache +0x18** | H.c `0x14041ED48-75`: once-only bind via `FUN_14041B9F0`; `0x14041ED78-8D`: `T = [owner+0x10]+0xC148`, cached at `+0x18` | flag 1 all 4; owner `0x1BC71DD0`; T_cache 0 (stale — the step uses the stack copy, see §4 INF) |
| `+0x998` | 0x28 side-1 bind block | same layout | same, second iter | flag 1; owner `0x1BC8FEE0`; T_cache 0 |
| `+0x988` | qword | H.e second teardown target | loop `0x14041EE9D-EF` ×2 over `+0x988/+0x9B0` | 0 all 4 (nothing bound) |

Setter/reader VAs: H.c bind `0x14041ED48-ED8D` (flag test, `FUN_14041B9F0` name-wire,
owner+0x10+0xC148); H.d loop `0x14041EDC0-EDDC`;
H.e planner call `0x14041EE57`; H.f `0x14041EF06/0x14041EF16-25`.
Per-side bind helper `FUN_14041B9F0` and `FUN_14041F010` are listener name-wire
fns (`call 0x141B55730` + `mov [rbx+8],1`), **not** T writers — the T bind is the
inline `0x14041ED7E-8D` sequence (CONFIRMED).

### 1.3 H+0x940 child (`0x0A3EEB60`, vtable `0x1425974B8`): INF, not read by the step

Head qwords identical in snaps 1–3 (`b8745942…`), second qword differs in snap1
vs snaps 2–3 (counter), so it carries per-frame/counter state, but **no H-step
instruction reads past `H+0x940+0x20`** — the child is opaque to S7 (OPEN: what
fills it; does not block the step port).

## §2 S record (the per-frame shared record)

### 2.1 Size, ctor, dtor, owner, lifetime

| Item | Value | Evidence |
|---|---|---|
| Size | **0x4B8** (inline at H+0x488) | CONFIRMED: inline ctor `FUN_140A45060(H+0x488)` (`0x14041E57F`); guard byte at S+0x4B0 (set `0x140A49D4D`, tested `0x140A49C56`/`0x140A484F0`, cleared `0x140A485D2`); furthest S field = S+0x4B0. S+0x488 (the flag block) lives at H+0x910, *outside* S — it is H's, not S's. |
| Ctor | `FUN_140A45060` (inline part) + `FUN_140A49C50` (per-match lane init) | CONFIRMED: `140A45060` zeroes/fills S+0x0..0x4B0 lane slots (§2.2 head); `140A49C50` runs the 40-flag lane init once (guard S+0x4B0). `140A49C50` has 11 other callers (setup/analyze paths, §7 scan) — per-match, not per-tick. |
| Dtor/teardown | `FUN_140A484F0` | CONFIRMED: called as the H tail-jump (`0x14041EF25`); clears lane/row bind slots (lane+0x18 test-and-clear `0x140A48536-8E`, row+0x18 × 0x16 `0x140A485A6-BE`) + S+0x4B0. 11 other callers (same family as S-init). |
| Owner | H (inline member) | CONFIRMED: never separately allocated; ctor/dtor take H+0x488. |
| Lifetime | per match object, per-frame content | CONFIRMED: guard S+0x4B0 = 0 in all 4 snapshots (teardown ran); lane vtables/objects + rows + flag block persist (identical across snaps); bind slots are cleared per frame (all zero in all 4 snapshots = post-teardown state). |

### 2.2 Field table (lanes = the S-init jump-table targets; teardown = the clear sites)

S-init (`0x140A49C50`) shape CONFIRMED: `rbp = S+0x488` flag bytes, k = 0..0x27;
only k with flag != 0 run. Jump table at `0x140A49D5C` (21 slots, resolved
`target = 0x140000000 + i32[table+k*4]` — the naive `u32` read gives garbage
`0x141493xxx`; the entry is a signed dword):

| S offset | Lane (k → callee) | Lane object (vtable / name) | Step use |
|---|---|---|---|
| `+0x0` | k=0 → `FUN_14087B3D0(S+0)` | `0x14259D590` ANALYZE | analyze-side record (OPEN vs H order per plan §1.1; persists across frames) |
| `+0x28` | k=2 → `FUN_14042A6D0(S+0x28)` | `0x142599550` CAMERA | camera record (render-side per IntegrationOpen §2.4) |
| `+0x50` | k=3 → `FUN_14042A740(S+0x50)` | `0x142599C28` PLAYER(rsp dump "PLAYER") | player-side record |
| `+0x78` | k=13 → `FUN_140426950(S+0x78)` | `0x142599A88` PAINPUT(rsp "PAINPUT") | input record |
| `+0xA0` | k=14 → `FUN_140A49040(S+0xA0)` | `0x14259DA60` RADOLL(rsp "RADOLL") | ragdoll record |
| `+0xC8` | k=17 → `FUN_1404DC3F0(S+0xC8)` | `0x142596A78` RECORD(rsp "RECORD") | record lane |
| `+0xF0` | k=15 → `FUN_140871CC0(S+0xF0)` | `0x1425AA960` BALLTATCH(rsp "BALLTATCH") | ball-touch record |
| `+0x108` | (no k; always zero) | null in all 4 | spare — zero in every capture |
| `+0x118` | 22 inline row objects, stride 0x28, k=20 → `FUN_140A48EF0` per row | vtable `0x1425AA900`, state +0x8, **bind slot +0x18** (`= S+0x130+j*0x28`) | rows persist per match (byte-identical snaps 1–2); bind slot +0x18 = 0 in all 4 (post-teardown) |
| `+0x130` | NOT a separate table — row j's bind slot (row_j+0x18; teardown `0x140A485A6-BE` clears all 22, `0x16` iters) | — | per-frame scratch, 0 in all 4 |
| `+0x18/40/68/90/B8/E0` | lane objects' +0x18 bind slots (teardown `0x140A48536-8E`: `if ([S+lane+0x18]) [S+lane+0x18] = 0`) — NOT cleared pointers | lane objects persist (identical all 3 same-boot snaps) | per-frame bind slots, 0 in all 4 |
| `+0x4B0` | byte guard | 0 all 4 | 0 = needs S-init; 1 = initialised. Per-match once, cleared per teardown — **the flag block at H+0x910 stays 0x01 so re-init is skipped; the guard still cycles** |

S+0x8 quirk (all 4): `0x3E584A27_00000001` in snaps (counter `0x3E584A27` + flag
`1`), `0x7F2A7538_7CE3AF01` live — per-frame counter + flag, writer OPEN (not in
the H chain; does not block S7 — S is passed opaquely to the tick/planner).

### 2.3 What S-init fills from where (per-frame vs per-match)

Per-frame S-init does **nothing after the first frame**: all 0x28 flag bytes are
0x01 in every capture, so `FUN_140A49C50` returns at `0x140A49D55` immediately
(guard S+0x4B0 is re-set to 1 each call). The lanes were filled once at match
setup from their own subsystems (lane objects carry their own vtables; their
content is byte-identical across the three same-boot snaps). Per-frame varying
state (counters, S+0x118 row +0x8 words, MI frame) is written by the
tick/planner/teardown, not by S-init. **For S7: build S once (ctor + one S-init),
then pass it through every frame; only the bind slots (lane +0x18, row +0x18,
S+0x8 counter) change per frame.**

## §3 Brain record (per-side tick context)

### 3.1 Size, ctor, dtor, owner, lifetime

| Item | Value | Evidence |
|---|---|---|
| Size | **0x6DF8** | CONFIRMED: H ctor `0x14041E62A mov edx,0x6DF8; call 0x1403FB410`, result stored `H+0x450+side*8` (`0x14041E657`); only image-wide E8 caller of the brain ctor `0x1404F0530` is `0x14041E647` (H ctor side loop). Furthest brain field in ctor/step: `+0x6D80` (§3.2); alloc covers it. |
| Ctor | `FUN_1404F0530` (via `FUN_1404F07A0`) | CONFIRMED: only image-wide E8 caller is `0x14041E647` (H ctor loop, sides 0..1); `edx` = side index. `1404F07A0` fills `+0x6B70..0x6D3C` defaults (`+0x6D00/+0x6D0C = 5`, `+0x6D18/1C = -1/0xFF`, `+0x6C50 = 0x3F000000 (1.0f)`, `+0x6D14 = 0`, `+0x6C40 = 0`); `1404F0530` sets vtable `0x1425C4880`, the four sub-objects at `+0x6D58/6D80/6DA8/6DD0` (heap vtables), `+0x6D48/+0x6D50 = 0`. |
| Dtor | H slot-0 path `0x14041E9CC` region (loop over `H+0x450` ×2, virtual calls) | CONFIRMED bytes (H dtor segment); exact brain-dtor VA OPEN (virtual dispatch, not traced — per-match only, does not block S7). |
| Owner | H (`H+0x450+8·side`) | CONFIRMED. |
| Lifetime | per match | CONFIRMED: allocated once in H ctor, freed in H dtor; the tick only writes per-frame fields (§3.2). |

### 3.2 Field table (every field the H step's chain reads or writes)

Brain-relative. "Tick" = `FUN_1404F6C30` body or its callees (plan §1.3 slice
list); "bind" = the per-tick `brain+0x6D48/6D50` stores at `0x1404F6C60/6A`.

| Offset | Width | Name | Writer / reader VAs | 4-capture values |
|---|---|---|---|---|
| `+0x0` | qword vtable | brain vtable `0x1425C4880` | ctor `0x1404F056A` | same all 4 |
| `+0x8` | s32/side | side index (ctor `edx`) | ctor; tick step 36 `MI+0x1198&1 == side` gate reads it (via `brain+8`) | 0 / 1 (side 1 live reads `0x36b3e79800000000` at +0x8 in the 8-byte view = side byte + adjacent; s32 view = 1 — width caution, value plausible) |
| `+0x10` | qword ptr | **ctx** (per-side context; `T_s` derived from it) | tick `0x1404F6CF0` (`lea rcx,[rdi+0x10]` → `FUN_14051C840` etc); `brain+0x6D48` points back at it | non-null all 4 (same addr snaps+live: setup-persistent) |
| `+0x134` | s32 | (brain-local word; NOT T+0x134 — do not conflate) | tick-internal | large/any (not a T word; listed so the task agent does not wire T+0x134 here) |
| `+0x6B70…0x6D3C` | mixed | ctor defaults block | `FUN_1404F07A0` (§3.1 values) | — |
| `+0x6C40` | s32 | stopped-select word (step 8 `FUN_1405008E0`) | writer `0x1405008F8` (`= 2` arm); reader step 9 | 3 snap1 / 2 live (varies = per-frame) |
| `+0x6CC4[11]` | f32×11 | support bank | step 11 `FUN_1404FEDC0` | `1.0` live, `-1.0` snap1-side1 (per-frame) |
| `+0x6CF4` | s32 | support stamp | step 11 | -1 / 10823 (per-frame) |
| `+0x6D48` | qword ptr | **per-tick ctx bind** (`= S`) | tick head `0x1404F6C60` stores arg2 (S); tick body reloads `0x1404F6CD3` (`[rdi+0x6D48]+0xE0` → `FUN_140A0C3E0` lane call) | live: S addr; snaps: `0x2C69A898` (S-time ctx — non-null = mid/post-tick snapshot) |
| `+0x6D50` | qword ptr | **per-tick T bind** (`= T_s`) | tick head `0x1404F6C6A` stores arg3 (T); body reads everywhere (`0x1404F6D19` `T+0x8c = 1`, `0x1404F6D41` `T+0x...`); cleared to 0 at tick end (step 48) | 0 in all 4 (snapshots taken outside the tick window — CONFIRMED per-tick transient) |
| `+0x6D58/6D80/6DA8/6DD0` | inline objs | four sub-objects (heap vtables) | ctor wires vtable + `+8/+10/+18/+20` zero-init + name-wire calls | same vtables all 4 |
| `+0xBCC` | s32 | mode source (T+0x134 := brain+0xBCC at `0x1404F6DBD`) | writer OPEN (plan-selector §2); reader tick copy | 2/3 (varies = per-frame input) |
| `+0xB8A2/A3/A4` | bytes/level | full-attack/defence flags + attack level mirror | tick `0x1404F6E1C-58` sets flags; `FUN_1404F8350` level | level 2 all sides/captures; flags 0 |

Not in the H step: `ctx+0xBA8` (P/designated-slot object) = 0 in all 4 captures —
the tick's ctx→P link is null outside the tick window (or this boot never builds
it); S7 passes ctx opaquely, does not need P (OPEN: P builder; does not block).

## §4 The binding sequence (what S-init does each frame)

Caller order CONFIRMED from `0x14041ECF0` raw (`0x14041ECF8–0x14041EF25`):

1. **S-init (H.a, `0x14041ECF8–ED02`):** `S = H+0x488`; `FUN_140A49C50(S)`.
   First frame: 40-flag walk fills the 8 lanes (§2.2) from their subsystems,
   sets S+0x4B0 = 1. Every later frame: flag block all 0x01 → returns immediately
   (CONFIRMED: all flags 0x01 in all 4 captures, mid-match). Fills **nothing
   per frame** — no MI/team/G-table fields are touched by S-init itself.
2. **MI check (H.b, `0x14041ED07–ED1C`):** `MI = *(G+0x270)` (no null test —
   MI must be non-null; null in ~91% of live samples per A4TeamTick §4.1, so the
   binder sets it just before H and clears it after — site still OPEN, S7 needs
   the harness/binder from S0); `if (*(u32*)(MI+0x1168) == 10) return` — skips
   the **whole frame** (S-teardown included). Values: 1 live, 3 snaps (never 10).
3. **T bind (H.c, `0x14041ED22–EDA5`):** per side s = 0, 1: if `H+0x970+0x28s`
   flag byte is 0, run the name-wire (`FUN_14041B9F0`/`FUN_14041F010` —
   `141B55730` + bind flag, writes nothing to T) once; then
   `T_s = *([H+0x970+0x28s+0x8] + 0x10) + 0xC148` (side owner → owner+0x10 → T).
   Stack copy at `[rsp+0x20+8s]` — **not** the `H+0x970+0x18` cache slot (which
   reads 0 in all 4 snapshots = stale/never re-stored; INF: written once at
   bind time, the step uses the stack copy — a task agent must use the stack
   copy too, not the cache slot).
4. **Team-tick loop (H.d, `0x14041EDC0–EDDC`):** s = 0, 1:
   `brain = H[0x450+8s]`; `if (brain) FUN_1404F6C30(brain, S, T_s)` with
   `rcx = brain, rdx = S (H+0x488), r8 = T_s` (CONFIRMED by the tick-head stores
   `0x1404F6C60/6A`: `[rcx+0x6D48] = rdx`, `[rcx+0x6D50] = r8`).
   s = 0 always runs first (no side test — CONFIRMED).
5. **Side planner (H.e, `0x14041EE57–EE62`):** `if (H[0x460])
   FUN_1404405D0(planner, S, {T_0, T_1}, …)`. Planner args CONFIRMED:
   `rcx = H+0x460`, `rdx = S`, `r8 = &T-array`, `r9 = H+0x940-derived`.
   Planner reads T+0x78 both sides, writes `T+0x3C…` per-slot rows,
   `T+0x2C = 0xFF` rows, `+0x10E4/10E5 = 0x18`, calls
   `140A571B0/571E0/54FE0/55000/555C0/555A0/57040/62690/489D0`;
   then the two `vt+0x88` callbacks on `H+0x940/H+0x988` sub-objects.
6. **Teardown (H.f, `0x14041EF06–EF25`):** `FUN_1405069A0(H+0x18)` with
   `rdx = S` (H+0x18 demo/out-of-play role writer; reads `H+0x18+0x8C/0x28/0x2C`,
   writes `+0x8C/+0x2C`, calls `140504D20/504990/506800/506660/5055E0/5B5230`
   — non-SIM, does not touch T/brain/MI); tail-jump `FUN_140A484F0(S)`
   clears the teardown slots + S+0x4B0 (§2.1).

Persist vs per-frame: H object, both brains (+0x6D48/6D50 excluded), planner,
S lane objects, flag block, S+0x4B0 re-armed each frame, T records, G tables —
**persist**. Per-frame: `brain+0x6D48` (S), `brain+0x6D50` (T, cleared at tick
end — 0 in all 4 snapshots CONFIRMED transient), S lane/row bind slots (+0x18)
+ S+0x8 counter, T tick words (§5), MI frame
counter. **A task agent holds H/S/brain/T/planner in PES layout once, and per
frame only re-binds S/T into brain+0x6D48/6D50, runs the loop, and tears down.**

## §5 Inputs from GF (for each field the step needs)

"GF holds" = this checkout (`src/`), S1/S2 in progress by a peer task agent.

| Step field | Value comes from (PES) | GF holds (this checkout) |
|---|---|---|
| S lanes (§2.2) | own subsystems at match setup (analyze/camera/player/input/ragdoll/record/balltouch) | nothing 1:1 (GF MentalImage/camera/player/input are functional analogues, different layout). S7 needs S only as an opaque PES-layout block built once by ctor+S-init — no GF feed per frame. |
| MI+0x1168 (skip == 10) | MI writer `0x1408AD0B0` (init), resets `0x141180540/0x141183710` (IntegrationOpen §4) | `matchPhase`/`inPlay` (`match.hpp:98` `IsInPlay()`, `matchclock.hpp` phase). Feed: GF match phase → word (stand-in `// ponytail:` names the writer VAs). |
| MI+0x116C (phase 1 = in play) | sole writer `0x141180540` reset path (IntegrationOpen §4) | `IsInPlay()` + `kPlayStateInPlay = 1` (`pesteamtick.hpp`). Same feed as above. |
| MI+0x1198 (frame) | copy `0x14100CCA0` refresh (IntegrationOpen §4) | `GetPesFrame()` (`match.cpp:505`, `PesFrameOf(actualTime_ms, gameSpeed=54)`) — CONFIRMED 1:1 by meaning. |
| MI+0x1554 (restart kind) | `0x140A3C560` / `0x141190F10` (IntegrationOpen §4) | `PesRestartKind()` (`match.cpp:508`, referee buffer → `RestartKindOf`) — partial stand-in. |
| MI+0x1775 (mode) | `0x140A3CAE0` ← referee slot-0 path (IntegrationOpen §2.3) | referee/`IsInSetPiece()` + `matchPhase` — stand-in, `// ponytail:` names `0x140A3CAE0`. |
| MI+0x1748[22] (on-pitch) | `0x1406D8F50`/`0x141E5ED10`/`0x1422BD460` family (IntegrationOpen §4) | squad on-pitch flags (roster holder). |
| T_s bind (owner+0x10+0xC148) | match side-owner objects (`0x1BC71DD0/0x1BC8FEE0` all 4 captures) | GF `Team` objects — the task agent owns the PES-layout T per side; the bind is the fixed `+0xC148` chain, no GF value needed. |
| T tick words (`+0x8C/134/E4/218/224/254/255/6E6/B6DE/B8A2/A3/A4/6A4/B860/B8A8/AC/6C40/B8B0-C0/B8D0/8C35/40/50/A2A0/28C/26C/78`) | previous tick + tick callees (plan §1.3) | `TeamTick` (`pesteamtick.hpp`: b8a4/a2/a3, t80, b8c0Frame, b6c40, reaction banks, support words, trapArmed) + `PesTeamRecord` sheet (`pesteamconcepts.hpp`: S at T+0x8E08) — S1/S2/S5 extend these into the record. Nothing else needed from GF: the tick computes them. |
| brain+0xBCC (mode source) | writer OPEN (plan-selector §2) | `modeBcc = -1` stand-in (`pesteamtick.hpp:55`, `// ponytail:` names `0x140501076`). |
| brain+0x6C40/6CC4/6CF4 | tick steps 8/11 | `TeamTick.b6c40/supportBank/supportStamp` — GF-owned already. |
| planner `+0x618/620/628/630/638` | H.e bind (`0x14041EE27-50`): `+0x620/628 = T_0/T_1`, `+0x630 = H+0x18-ctx`, `+0x618 = r9 (H+0x940 chain)`, `+0x638 = 0x141E5D4C0` result | T pointers (owned) + ctx (owned); `+0x638` recomputed per frame by the planner head itself. |
| G+0x180 roster / G+0x270 MI liveness | per-tick binder (OPEN, A4TeamTick §4.1; S0 harness owns) | GF squads + referee/clock (S3 `PesMI` owns when it lands). |

## §6 The stage's proof (S7 agreement test)

Purpose: prove a GF-held H/S/brain/T in PES layout, run through the translated
step, behaves like PES. Blocked items named per test (unmodellable
`1404F6C30/1404FEDC0` per plan §4 FLAG — the tests below are designed to run
with stubs first and flip to the translated callees as S6 lands them).

- **T1 H/S/brain layout agreement (runs now, no translated code needed).**
  Build H, two 0x6DF8 brains, S (0x4B8), planner, T pair from any of the 4
  snapshots' bytes; assert: sizes (H 0x9B8, S 0x4B8, brain 0x6DF8),
  vtables (`0x142597580/0x14259D590…/0x1425C4880/0x14259BD90`),
  S-init lane objects, T-bind chain (`owner+0x10+0xC148` == snapshot T),
  MI skip word != 10, `brain+0x6D50 == 0` (outside-tick invariant).
  Data: the 4 snapshot readers in scratch (`fsnap.py`/`read.py`).
- **T2 Binding-sequence trace agreement (runs now, harness-level).**
  Single-step the H.a–H.f call order against the VA sequence
  (`140A49C50 → MI check → T bind → 1404F6C30 ×2 → 1404405D0 → 1405069A0 →
  140A484F0`); assert arg values (`rdx = H+0x488`, planner `r8 = &{T0,T1}`,
  `brain+0x6D48/6D50` set then `6D50` cleared). Uses the S0 frame-trace
  harness when it runs; until then the VA order above is the oracle.
- **T3 Team-record fields after one step (needs S5 slices + S6 callees).**
  From a snapshot (e.g. snap1, frame 30381), run the translated step on the
  same state; compare the plan §1.3 SIM words
  (`T+0x8C/134/E4/218/224/254/255/B8A2/A3 + brain+0x6C40/6CC4/6CF4/6D44`).
  Pass bar: identical for the red-test slices (S5 per-slice tests), then full
  agreement as S6 callees land. First divergence tick + causal chain reported
  per the differential-testing rule (plan §4 proof key A+R).
- **T4 Planner agreement (needs translated `1404405D0` diffed — currently
  translated-but-undiffed, "harness cannot run").** From a snapshot, run the
  planner on `{T_0, T_1}`; compare `T+0x3C…` rows, `T+0x2C = 0xFF` rows,
  `+0x10E4/10E5`, and the `T+0x78` pair snapshot (the planner reads but does
  not write `T+0x78` — assert it is unchanged).
- **T5 Teardown agreement (runs now).** Run translated-or-ported
  `FUN_140A484F0` on a snapshot S; assert the bind slots (lane +0x18, row +0x18)
  + S+0x4B0 are zero and lane objects/vtables/flags/row state are untouched.

Fidelity metrics (per the recreation rule): divergence tick, RMS position
error on `man+0x55C/564` after N steps, matching event sequences, identical
final scores over N seeds — T3/T4 report the first two; the score identity
needs S8 (per-player driver) and stays OPEN.

## §7 Open items (ranked, S7-blocking first)

1. **Unmodellable `1404F6C30` + `1404FEDC0`** (translation pipeline owns;
   plan §4 FLAG). S7 cannot run PES's step until these are modellable or
   stubbed per-slice — T3 is designed to land slice by slice behind them.
2. **Per-tick MI/roster binder set/clear site** (A4TeamTick §4.1; narrowed by
   IntegrationOpen: the `0x14100CCA0` copy + 7 callers is the refresh path;
   which caller runs per tick needs S0 frame-trace coverage). S7 needs it to
   keep MI non-null across frames.
3. **H+0x970+0x18 T_cache staleness** (INF §4.3: cache slot 0 while the step
   uses the stack copy — one live read inside the tick window settles whether
   the cache is ever re-stored; S0 coverage answers it for free).
4. **`FUN_14041EC80`** (the decompile-named H body; raw H is `0x14041ECF0` —
   the two VAs name overlapping regions; the task agent should treat
   `0x14041ECF0` as authoritative and note `0x14041EC80` as the dtor-entry
   alias, not a second function).
5. **S+0x8 counter/flag writer** (OPEN §2.2; per-frame counter, not read by the
   tick/planner/teardown — S7 passes it through opaquely).
6. **H+0x940 child contents + `ctx+0xBA8` P builder** (OPEN §1.3/§3.2; opaque
   to the step — S7 does not need them).
7. **Brain-dtor exact VA** (virtual dispatch in H dtor; per-match only).
8. **`140A49C50`/`140A484F0` 11 non-H callers** (setup/analyze paths — S7 uses
   the H call sites only; the others are context, listed for the S9/analyze
   agent).

## §8 What was verified, what was not, budget

- Verified this pass: H size/ctor/dtor/owner (alloc/dealloc immediates + scans +
  snapshots), H field table (§1.2, 4 captures), H vtable + H+0x940 vtable +
  child vtable (image qwords), S size/ctor/teardown/guard (disasm + 4 captures),
  S lane map (jump-table resolved as signed dwords + lane vtables/names +
  4-capture identity), S flag block (all 0x01), teardown semantics corrected to
  bind-slot clears (lane+0x18/row+0x18, not cleared pointers), brain size/ctor/
  owner (single-caller scan + 4 captures), brain field table (tick-head stores +
  4 captures), T-bind chain (owner+0x10+0xC148 == T with T+0x78/8C/134 sane in
  all 4), planner args + T+0x3C/2C/10E4 selectivity (disasm), MI words
  (4 captures), GF holder map (§5, grep over `src/` at this checkout), T
  far-read (0xC200 in-bounds = T alloc ≥ 0xC200; GF `0xC138` covers spec'd words).
- Not verified: brain-dtor VA (virtual), H+0x940 child contents, S+0x8
  writer, P builder, per-tick MI binder (all OPEN above with next steps).
  No GF code changed (read-only assignment); no build/tests run (decode-only).
- Budget: ~100 tool requests of ~150; report written to
  `pes-re-s7-records.md` (this file).
