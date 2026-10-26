# PES S4: MA nearest-list builder + two live values (S4Builder, 26-10-26)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA - 0x140000000).
Labels: **CONFIRMED** = disassembly bytes read this pass (capstone over the image) or live/capture bytes read this pass;
**INF** = inference; **OPEN** = narrowed remainder with the exact next step. No PES bytes quoted.
Scratch: `<workdrive>/gf-S4-scratch/` (`live1.py`, `live2.py`, `live3.py`, `polldiv.py`, `pollsetup.py`).
Ghidra bridge (`:8080`) down all session; all static evidence is capstone disassembly. Live reads: read-only
`sudo -n` pread of `/proc/53052/mem`, logged with times; never write/pause/signal/attach. PES pid 53052 throughout.

Prior docs re-checked, not re-derived: `docs/re/pes-re-integration-open.md` (esp. §3),
`docs/re/pes-re-integration.md` (§1.3/§2/§5), `docs/re/pes-re-teamtick.md` (§3).

## 0. Headline verdicts

| # | Ticket item | Verdict |
|---|---|---|
| 1 | MA nearest-list builder (`MA+0x22E4C + 200·i`) | **OPEN, narrowed to a named unknown.** Layout + inputs + frame window CONFIRMED live; writer still unidentified after profiling ~70 functions. Next step named in §3. |
| 2 | Frame position + inputs | **CONFIRMED bounds + inputs** (§4). Exact tick slot stays with the writer. |
| 3a | Divisor `[0x1434456B8]` + type base | **CONFIRMED values, corrected object** (§5). Divisor = 0 in play and in steps (360+ reads); type base is `[setupobj+0x100]`, not `[M+0x100]`. |
| 3b | Hub object + `+0x1820` default | **OPEN, narrowed.** Owner chain mapped to `ctx+0x10` (§6); hub not located in M/G/H blobs. |

## 1. The `+0xB2` rank consumer back-trace (ticket start point)

- `0x140538050` (72 insns, ends `ret` at `0x14053815a`) is the mode dispatch: `ebx = [rsi+0xb8b4]`
  switch (`sub ebx,1/2/...`, 5 arms) calling `0x1405387a0` / `0x140539260` / `0x140539af0` /
  `0x140538160` / `0x140539860`. Direct E8 callers image-wide: **one** (`0x1404f78cf`,
  inside team tick `0x1404f6c30`, step ~27a-55 region). CONFIRMED (E8 scan).
- None of the five dispatch targets reads `+0xB2` literally (scan CONFIRMED). They sort
  `{id, d²}` pairs on **stack scratch** (`[rbp+rax*8-0x50]` / `[rbp-0x50]`, counts at
  `[rbp+rdx+0x60]`, `sort 0x140a79640`) and pick best candidates with comiss tie-breaks
  (`0x140538f85..0x140538fa1`: abs + `comiss` + `cmova`) — e.g. `0x1405387a0` head
  (`mov r11,rsp; lea rbp,[r11-0x168]` ... ball reads `[rbx+0x2482c/34]`, on-pitch
  `[rsi+0x1490]`-gated loop, sorts at `0x140538bd1`/`0x140538e7e`). These are **list
  consumers**, not the builder. The ticket's back-trace therefore ends at readers;
  §3 takes the forward (builder-shape) search instead.

## 2. Layout + contents CONFIRMED live (two frames + one live read)

- Row `i` at `MA+0x22e4c+200·i`: team 0 `{u32 id, f32 d²}×11` at `+0x00`, team 1 at `+0x58`,
  `u8 count` at `+0xB0/+0xB1`, `u8 rank` at `+0xB2+id` (absent = 10). Owner `i` excluded
  from its own side (`0xFF` sentinel fills the last own-side slot).
- f13/fr28455 vs f19/fr29677 (`cap2` captures, same MA VA `0x7ff5fd3b0018`): all
  **231/231 entries recompute** (`d²=(x_j−x_i)²+(z_j−z_i)²`, `x=man+0x55c`, `z=man+0x564`)
  in **both** frames, ascending within each side. Field correction this pass: `z` is
  `man+0x564`, not `+0x55c+4`-adjacent misread (`+0x560` = y ≈ 0). Sorted order differs
  between frames consistently with position changes (e.g. row 0 t0 head `2,1,4` → `2,3,7`).
- Live (13:23 UTC, pid 53052, MA `0x7ff5fd3b0018` via `W=*(M+0x118)` vt `0x14259d4f0`,
  `blk=*(W+0x10)`, `MA=blk+8` — the tasks/14-10-26 chain re-verified): row 0
  `t0[0] id=1 d²=609.6`, counts `10 11`, ranks `[10,0,1,6,2,5,...]`.
- No exact `d²` ties in either capture (0 ties over 44 rows), so the tie rule is not
  observable in this data; the writer's tie behaviour stays INF (insertion/sort order).

## 3. Writer hunt: excluded regions (all CONFIRMED reads, capstone)

Method: (a) whole-code `imul …,0xC8` census = **403 sites**; (b) per-function true-extent
scans (heads delimited by `CC` runs) for 200-stride + float-`d²` + sort + count/rank
stores; (c) image-wide store-pattern scans: **0** stores with disp in pair span
`[0x22e4c,0x23f70]`, count/rank span `[0x22efc,0x23f7b]`, or rank `0xB2`-indexed form —
the writer stores through a fully computed base (small-displacement stores only).

Excluded as readers/consumers (no 200-stride build, stores are stack/team-table/brain):
MA-build chain `0x140535c60` family (`0x1405346e0`, `0x140531490`, `0x140531260`,
`0x140531810`, `0x1405325e0`, `0x14052f4f0`, `0x14052f720`, `0x140532f70`,
`0x140530320`, `0x140536fc0`, `0x140535a10`, `0x140535350`, `0x140535650`,
`0x140533c80`, `0x140533d10`, `0x1405360d0`, `0x140536170`, `0x140536210`,
`0x140536690`, `0x140634470`, `0x140635ab0`, `0x140628400`, `0x1405379c0`,
`0x140537ac0`, `0x140537bf0`, `0x140534a20`); per-player chain (9:
`0x140925470`, `0x14092fad0`, `0x14092af10`, `0x140926890`, `0x140928fc0`,
`0x1409257b0`, `0x140929a00`, `0x14092a340`, `0x14092c9f0`, plus `0x1421019d0` /
`0x142110d30` stubs); dispatch family (§1); tick callees `0x14053c6e0`,
`0x140539ed0`, `0x140a408c0`, `0x140509170`, `0x14057ba50`, `0x140520930`;
20 sort-`0x140a79640` call sites profiled (all stack-scratch lane/candidate builders,
e.g. `0x1404f9244`, `0x14052cd35`, `0x14053840b/bd1/e7e`); generic sorter `0x140a40ca0`
(8 E8 callers, all stack/team-table destinations).

**OPEN remainder (named unknown `S4-WRITER`):** the 22-row `{id,d²}+counts+ranks`
builder. Not in the MA-build chain (zero `0xC8`-stride in `0x140533c80..0x140538050`),
not in the per-player chain, not in the tick callees above. Unprofiled locations with
highest prior: analyze-listener family `0x14042b9f0` + 10 steps (`0x140426a90`,
`0x1404267a0`, `0x140426950`, `0x1404267f0`, `0x140426810`, `0x140426860`,
`0x140426760`, `0x140426cd0`, via `0x140a0d3f0/0x140a0d520/0x140a0d190/0x140a3e140/
0x1404582c0` — all undecoded per plan §1.2) and the `0x1404f8c70` impure region's full
extent. Next step: decompile those, or catch the writer dynamically (write watchpoint —
needs owner approval, NOT done: read-only rule).

## 4. Frame position + inputs (CONFIRMED bounds)

- Readers run in the team tick (`0x1404f62fb`, `0x1404f9c04/35` in `0x1404f6c30`) and in
  ~200 decision sites; the mode dispatch reader `0x140538050` is called at `0x1404f78cf`,
  i.e. **after** the MA-build call `0x140535c60` at `0x1404f7955` in tick order — so the
  tick reads the list built on the **previous** tick (lag 1; positions move ~0.05 m/tick,
  hence 231/231 recompute matches either way). Writer runs once per frame before/at the
  tick window; exact slot goes with `S4-WRITER`.
- Inputs CONFIRMED live: `man+0x55c/0x564` (x,z) recompute exactly (§2); on-pitch
  `MI+0x1748[22]` = all 1 in both captures (open play) with rows containing all 22 ids
  (owner excluded from own side). A4 §3.2's "off-pitch absent" cause stays INF(h).

## 5. Live value 1: divisor `[0x1434456B8]` + type base (CONFIRMED, corrected)

- Encoding CONFIRMED: `0x140418a35`: `F7 35 7D 2D 03 30` = `div dword [rip+0x302d47d]`,
  target `0x1434456B8` (capstone; the open doc's byte string stands corrected).
- Registrar `0x140418940` shape CONFIRMED: `edi=[r12+0x100]+1` ... 22-iteration loop
  (`sil` 0..10 inner, `rbx` 0/`0xb` outer, counter `[rbp+0x58]++`, `div` gate, pair
  register at `edi,edi+1`, `edi+=2`); sole E8 caller `0x14041ac0e` (state machine
  `0x14041aae0` = M slot 1, CONFIRMED vtable). `r12` = setup-state object (`[rbx+8]`
  state, `[rbx+0x100/0x5d8/0x770/0x7a0]`), **not M** — so the paired type base is
  `[setupobj+0x100]`, and `[M+0x100] = 0x04020000` (live, constant 13:05→14:43 UTC) is a
  different word. Do not port against `[M+0x100]`.
- Live values: `[0x1434456B8]` = **0** during play and during steps (frame 9271,
  14:37 UTC; frames 30304/30381; 360-read poll over 12 min + 100s sampler — all 0).
  The gate divisor is nonzero only inside the setup window (else the `div` would fault);
  no setup ran during this session (no new G in 200+s watches). Next step: read both
  words in a setup window (watch for G change, then read `[0x1434456B8]` +
  `[setupobj+0x100]` immediately; setup object = state-machine `rbx`, `[rbx+8]` ∈ {4,5}).

## 6. Live value 2: hub object + `+0x1820` default (OPEN, narrowed)

- Owner chain mapped (CONFIRMED): listener factory `0x140418710(r13, edi)` registers
  into hub `rcx=[rsi+0x10]=[r13+0x10]` via `vt+0x30` (`call [rax+0x30]` at `0x140418796`);
  `r13` = setup-state-machine 2nd arg (`rdi=rdx` at `0x14041ab08`, state object `rbx=rcx`).
  So hub = `[ctx+0x10]`, `ctx` = the slot-1 dispatch's `rdx`.
- Hub NOT found live: scanned M+0x0..0x1000, G blob, H+0x0..0x1000, M+0x1000..0x3000 for
  hub vtables `0x142ac1cb0/1d48/1de0` (hub slots per open doc §2.1: `+0x30→0x141b3db70`,
  `+0x38→0x141b3db40`, `+0x40→0x141b3dc50`, `+0x48→0x141b3dc90`; dispatch `0x141b3edd0`
  order `+0x20`-then-`+0x1820` re-verified by shape) — no hits. Hub vtables have **zero**
  `lea` refs image-wide (Player vtable has 2: `0x14091ea36`, `0x14092000c`), i.e. the
  hub ctor installs its vtable without a direct lea (template copy or register-carried).
- `M+0x10[0..21]` are the 22 `match::Player` objects (vt `0x1426329a8`, `[obj+0x10]` =
  counter e.g. 30039, `[obj+0x18]` = index 0..21 — live 14:19 UTC), **not** the hub;
  the open doc §2.2 phrase "match hub object M+0x10" is corrected accordingly.
- `+0x1820` default content therefore unread this session. Next step: read it in a setup
  window via `[ctx+0x10]+0x1820+t*8` (t=0..3), or via a freshly registered listener's
  hub back-link; the default is expected constant (ctor-set, dispatch relies on non-null).

## 7. Open items (ranked)

1. `S4-WRITER`: 22-row builder VA (§3; analyze-listener family first).
2. Setup-window live reads: `[0x1434456B8]` nonzero + `[setupobj+0x100]` + `[ctx+0x10]`
   hub `+0x1820` table (§5–§6 method; needs a match setup, ~2 s burst read).
3. Tie rule on exact-`d²` ties (no ties in 44 rows; needs a set-piece/cluster capture).
4. Handover: S4 stays BLOCKED (ships as known-approx writer); S8/S9 notes in
   `pes-re-integration-open.md` §§1–2 unaffected (registrar/hub findings above refine,
   not contradict, them).
