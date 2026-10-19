# PES lane inputs: writers/meanings behind the five silent A4 carrier units (decode, 19-10-26)

Scope: close Track A. Probe: `pes-port-A4Lanes3.md` (branch `A4Lanes3`, `43c16ae`): OneTwo dies at one-touch G3 + receiver P2 ψ; DHB at `K+0x6E`; BackSpace at the corner/curve gate; Safety + G5/DHB-score at score row S1; GoalFront at picker inner gates. Specs: `pes-re-lane-units2.md`, `pes-re-carrier-helpers.md`, `pes-re-lane-structs.md`, `pes-re-image-rows.md`, `pes-re-lane-core.md`, `pes-re-role-writer.md`. Image `pes_full.bin`, base `0x140000000`. Read-only: bridge dec/dis + `objdump -D -b binary` + byte scans. No GF edits, no live PES, no commits. Scratch: `<workdrive>/gf-LaneInputs-scratch/` (`dec_*.c`, `scan*.py`, `floats.py`).
Owner ruling 19-10-26 (brief-rules): floats below are script-read from the image; every cited constant pairs instruction VA with operand VA. No dt18 values used.

Legend: **CONFIRMED** = raw asm read this session (VA cited). **C-dec** = bridge decompile. **INF** = named inference. **OPEN** = blocked, next step named.

## Verdict

| # | Input | Verdict | Unblocks |
|---|---|---|---|
| 1 | S1 score/list/angle rows `MA+0x228EC..0x22CBC` + NearestList `MA+0x22E4C` | Writer **OPEN**; selectors, MA load, row layout CONFIRMED; all team-tick callees excluded as writers (§1) | Safety length, OneTwo G5, DHB score, GoalFront picker (§6) |
| 2 | Team level/mode (one-touch G3, O12/S4) | Reader, G3/G4 gates, both setters, sync-copy site CONFIRMED; value = level×100, gate `>= 200` (§2) | OneTwo (G3); also G8 cap, snapshot `[0x11]` |
| 3 | ψ (S6) receiver P2 | **CLOSED**: ψ = role class (`T+0x4BD4`), gate + writer store CONFIRMED in raw asm (§3) | OneTwo receiver predicate |
| 4 | `K+0x6E` | Copier `0x140AC0280` CONFIRMED; K identity CONFIRMED; ultimate writer OPEN (§4) | DHB |
| 5 | BackSpace curve gate | **Covered** by image-rows §2.3 writer `0x142138220` + exact `CurveEval` (§5) | BackSpace `0x1421225E0` |

## 0. Float table (script output, `struct.unpack('<f')` at VA-0x140000000)

| VA | bits | value | paired instruction this report |
|---|---|---|---|
| `0x142590D98` | `0x3E99999A` | 0.30000001192092896 | (prior docs; frames `int(54·0.3+0.5)=16`) |
| `0x142565600` | `0x3F000000` | 0.5 | (prior docs) |
| `0x142598580` | `0x43340000` | 180.0 | (prior docs) |
| `0x14259BDEC` | `0x40C00000` | 6.0 | (prior docs) |
| `0x142588218` | `0xBF800000` | -1.0 | (prior docs) |
| `0x14256AE1C` | `0x3F800000` | 1.0 | (prior docs) |
| `0x14259858C` | `0x43B40000` | 360.0 | (prior docs) |
| `0x14259BE5C` | `0x42B40000` | 90.0 | (prior docs) |
| `0x142598598` | `0xC3B40000` | -360.0 | (prior docs) |
| `0x14259A760` | `0x420C0000` | 35.0 | (prior docs) |
| `0x14259BE4C` | `0x428C0000` | 70.0 | (prior docs) |
| `0x14259BE84` | `0xC1200000` | -10.0 | (prior docs, G12 band floor) |
| `0x14259CE40` | `0x44C80000` | 1600.0 | (prior docs, G5 floor) |
| `0x14259BDD0` | `0x3F31C71C` | 0.6944444179534912 | (prior docs) |
| `0x14259BE0C` | `0x41700000` | 15.0 | `0x142124A97 MOVSS XMM8,[RIP+…]` → `[0x14259BE0C]` (objdump comment; this session) |
| `0x142565604` | `0x40000000` | 2.0 | (prior docs) |
| `0x14259A764` | `0x42700000` | 60.0 | (prior docs) |
| `0x14259222C` | `0x40A00000` | 5.0 | (prior docs) |
| `0x14259D60C` | `0x41100000` | 9.0 | (prior docs) |
| `0x14259BE70` | `0x43610000` | 225.0 | (prior docs) |
| `0x1425C65F4` | `0x44610000` | 900.0 | (prior docs) |
| `0x14253EE30` | `0x42C80000` | 100.0 | (prior docs) |
| `0x1425656C0` | `0x40800000` | 4.0 | (prior docs) |
| `0x14259BE20` | `0x41C80000` | 25.0 | (prior docs) |
| `0x142588200` | `0x3DCCCCCD` | 0.10000000149011612 | (prior docs) |
| `0x142596E6C` | `0x42A00000` | 80.0 | (prior docs) |
| `0x14259BE1C` | `0x41A00000` | 20.0 | (prior docs) |
| `0x14258C264` | `0x40400000` | 3.0 | (prior docs) |
| `0x14256AE30` | `0x42F00000` | 120.0 | (prior docs) |
| `0x14259D628` | `0x42DC0000` | 110.0 | (prior docs) |
| `0x1425A9BF0` | `0x43020000` | 130.0 | (prior docs) |
| `0x1425C4900` | `0xC28C0000` | -70.0 | (prior docs) |
| `0x142588210` | `0x41200000` | 10.0 | (prior docs) |
| `0x1425A9464` | `0x42100000` | 36.0 | (prior docs) |
| `0x14259BE40` | `0x42652EE0` | 57.2957763671875 | (180/π; `0x14045B090` bearings) |

## 1. S1: sign-selected score/list/angle blocks + NearestList (writer OPEN, everything else pinned)

### 1.1 Selectors (CONFIRMED, raw asm this session)

- `0x140A714A0`: `0x140A714A0 LEA RAX,[RCX+0x22AD4]`; `0x140A714A7 CMP DL,1`; `0x140A714AA JE`; `0x140A714AC LEA RAX,[RCX+0x228EC]`. So k==1 → `T18+0x22AD4`, else `+0x228EC`.
- `0x140A714C0` (score reader base): `LEA RAX,[RCX+0x228EC]` first (k==1 → `0x228EC`, else `0x22AD4`) — the flipped pair. C-asm (objdump head).
- `0x140A714E0` (list row): k==1 → `+0x22B9C` else `+0x229B4` (C-dec, bridge). `0x140A71260` (angle): k==1 → `+0x22C64` else `+0x22A7C` (C-dec). `0x140507740` (NearestList row): `slot<22 ? T18+0x22E4C+200·slot : T18+0x22E4C` (C-dec).
- E8 caller counts (image-wide rel32 scan, CONFIRMED): `0x140A714A0` 58 callers, `0x140A714C0` 20, `0x140A714E0` 110, `0x140A71260` 80, `0x140507740` 41, `0x140A79640` (rank builder) 73.

### 1.2 MA object and base (C-dec + E8 chain CONFIRMED)

- Team-AI tick `0x140457070` (C-dec, `dec_0x140457070.c`): `lVar3 = *(param_1+0x508)` at lines 72/113 and throughout; every `MA+0x2xxxx` access hangs off `lVar3`. So **`MA = *(teamTickThis+0x508)`**.
- Call chain (E8 scans): `0x140457070` ← single caller `0x140458940` (inside `0x140458880`); `0x14045B090` (ball-list writer) ← `0x140457568`; `0x14045E0D0` (gated second NearestEntry builder, writes `MA+0x24840/0x24908` lists, NOT our rows) ← `0x1404575FE`. All three callees fire from inside the team-AI tick.
- Where the `+0x508` pointer itself is allocated: OPEN (allocator not traced). The `T18 = MA` link (`*(X1+0x18)` vs `*(this+0x508)`) stays INF (image-rows §3.0).

### 1.3 Row layout (CONFIRMED shape)

Each 200 B row = `team[2][11] {u32 id; f32 d2}` + `u8 count[2]` at `+0xB0` + `u8 rank[22]` at `+0xB2` (absent rank `0x0A`, empty id `0xFF`). CONFIRMED by the ball-list writer `0x14045B090` (full C-dec read: init loops store `0xFF` ids, `0x0A` ranks, `0.0` angles; entries `d2 = (ball−p)²`, ranked by `0x140A79640`) and by live teamtick §3.1 for the NearestList. The tiling `0x229B4+0xC8=0x22A7C … 0x22D84+0xC8=0x22E4C` is CONFIRMED arithmetic (image-rows §2.5).

### 1.4 Value formula: OPEN (owner point unknown)

`score(k,s)` = the `d2` float of slot `s`'s own entry at its rank in the sign-selected row. The row's owner/centre point (which point the `d2` is measured from) is **not** the ball: the ball has its own row (`MA+0x2470C`, writer `0x14045B090`). Adjacency INF (lead only): the goal-geometry block `MA+0x228D4..0x228E8` (ball→goal headings/`d2`, writer `0x14045B090`/`0x14045DF70`) immediately precedes the score row — same builder region, different writer. Do not port against this.

### 1.5 Writer sweep: all reachable team-tick callees excluded (C-dec each)

Decoded and checked for stores to `0x228EC/0x22AD4/0x229B4/0x22B9C/0x22A7C/0x22C64/0x22CBC/0x22D84/0x22E4C` or `0x140A79640` calls writing them: `0x140459CE0`, `0x14045A290`, `0x14045AB40` (writes `MA+0x24A3C` list only + rank call on it), `0x140461420`, `0x14045BFE0`, `0x14045DDB0`, `0x14045C230`, `0x14045D8B0` (**NearestList reader**: builds only stack verdicts), `0x14045C960` (list-row `0x140A714E0` **reader**), `0x14045AFA0`, `0x14045BEF0`, `0x14045D810`, `0x140458B20`, `0x1404626A0`, `0x1404623D0`, `0x140534A20`, `0x140536210`, `0x140536690`, `0x140634470` (**reader**), `0x140635AB0` (**reader**), `0x140633940` (**reader** via computed base `local_120+T18 + slot·200+0x22E4C`, C-dec lines 110–117 — this confirms the computed-base read pattern the static store scan misses). No writer found. The 73 `0x140A79640` callers are NOT all excluded (only the ones above were read); the writer most likely ranks a stack-built list and copies it through a computed base.

### 1.6 Port statement / missing GF state

```cpp
// Layout is portable (GF HAS the shape via BuildNearest/NearestList):
// score(k,s) = row(k)[rank(k,s)].d2, row(k) = 200B NearestEntry row.
// Safety length = sqrt(row(!k_carrier? opposite-sign row)[carrierRank].d2)  // 0x140A714A0 select, 0x1415A1780
```
Missing GF state (exact): the owner point of the sign-selected rows + writer cadence. Until then the Safety-length stand-in (nearest-opponent distance + radius), the G5/DHB-score stand-in (goal-centre `d2`), and the GoalFront/list/angle stand-ins stay, each citing `0x140A714A0`/`0x140587E70`/`0x140A71260`/`0x140A714E0`.
Unblocks when found: **Safety** (exact length), **OneTwo** (G5 1600), **DHB** (225/900), **GoalFront** picker (§6).

## 2. Team level/mode read by one-touch G3 (+G4/G8) (readers, gates, setters CONFIRMED; ultimate value source INF)

### 2.1 Level reader `0x140A40BD0` (CONFIRMED raw)

`0x140A40BD0 CMPB [RCX+0xA29C],0`; `0x140A40BE0 JE 0x140A40BF5`; zero path via `CALL 0x140A602B0`; `0x140A40BF5 MOV EAX,[RCX+0xA294]`. So `level = (+0xA29C ? 0 : +0xA294)`.

### 2.2 G3 gate (CONFIRMED raw)

`0x142128CF7 CALL 0x140A40BD0`; `0x142128CFC MOV R14D,2`; `0x142128D08 CALL 0x140A60360`; `0x142128D0D TEST AL,AL`; `0x142128D0F JNE 0x142128CD5` (reject). `0x140A60360` = `*p/100 < arg` (C-dec; raw head shows the `/100` magic `MOV EAX,0x51EB851F; IMULL (%RCX); SAR 5`). So **G3 = `level/100 >= 2`**, i.e. **level ≥ 200** with the zero-flag clear.

### 2.3 G4 mode gate (CONFIRMED raw)

`0x141052E30` = `*(M+8)` (C-dec: `return *(param_1+8)`). `0x142128D15 CALL 0x141052E30`; `CMP EAX,3; JE pass`; second call; `CMP EAX,4; JE pass`; else falls to the `+0x4C00` category test. So **mode ∈ {3,4}**.

### 2.4 Writers (CONFIRMED raw)

- `+0xA294` setter `0x140A424E0`: `0x140A424E0 MOV [RCX+0xA294],EDX; RET`. E8 callers include `0x14044AE8D`, `0x140454F9E/FB3`, `0x141FBA47D`, `0x141FBE5AA/694/6E0`, `0x141FCDECD`, `0x141FCF3ED` (value passed in EDX at each site; ultimate source = engine/difficulty object via `0x14044A9D0`←`0x1404462AC` path, INF past the copy `0x140FC7ED0` which is a trivial `*dst=*src`).
- `+0xA29C` setter `0x140A428B0`: `MOV [RCX+0xA29C],DL` (objdump tail at `0x140A428B0`).
- Sync copy (CONFIRMED raw, `0x140416FBE–0x140416FDD`): `MOV EAX,[RSI+0xA294]; MOV [RDI+0xA294],EAX` (+`0xA298`, `+0xA29C` byte) — team objects copy level fields wholesale; GF must treat level as synced per-team state, not per-tick computed.
- Observed value scale (C-dec, one-touch G8 + snapshot): thresholds `level/100 ∈ {2,4,5,6}` → level lives in the hundreds (e.g. G8 `base = 2 if /100∈{5,6}, 1 if ==4`). Exact enum (what 200 vs 600 means): OPEN.

### 2.5 Port statement / missing GF state

```cpp
// Pure over GF state once GF carries it:
bool OneTouchG3(int teamLevelX100, bool levelZeroFlag) { return !levelZeroFlag && teamLevelX100/100 >= 2; }
```
Missing GF state (exact): per-team `level×100` int + zero flag byte + mode enum (`M+8`); writers `0x140A424E0`/`0x140A428B0`/`0x141052E30`-source. Unblocks: **OneTwo** (G3 is its live closer); also feeds G8 cap and snapshot `[0x11]`.

## 3. ψ in the receiver predicate (CLOSED)

### 3.1 Gate (CONFIRMED raw)

`0x142124A83 CMPL $0,0x54(%RSP)`; `0x142124A8B JE 0x142124AD8` (reject). The predicate frame zeroes a local snapshot at `[RSP+0x20]` (`LEA RCX,[RSP+0x20]; CALL 0x14063C560` at `0x1421249E4/9EF`), so `RSP+0x54` = snapshot+0x34 = snapshot dword `[0xD]`. **ψ = snapshot(r)[0xD]; ψ==0 rejects.**

### 3.2 Writer store (CONFIRMED raw, `0x14063C9F0` via `0x14063C560`)

`0x14063CBEA MOV ECX,[R14+RAX*4+0x4BD4]` (k = slot%11 path; else `MOV ECX,[R14+0x4BD4]` at `0x14063CBF4`); `0x14063CC06 MOV [RSI+0x34],ECX` where R14 = `T(side)` and RSI = snapshot. So **ψ = `T(side)+0x4BD4[k]`**.

### 3.3 Meaning (CONFIRMED, existing role-writer doc)

`T+0x4BD4` = `S+0x3F8C` = role-class array 0..9, rebuilt every team tick from the sheet registry through the dumped class map (role 0 GK→0 … 12 CF→9; `pes-re-role-writer.md` §§1–5, DONE). Keeper selector returns first active class-0 entry. So **ψ≠0 ≡ receiver is not the keeper** (bulk value; post-`0x140A422C0` B-swap caveat does not touch A).

### 3.4 Port statement (pure over GF state NOW)

```cpp
// Receiver predicate P2, exact:
bool ReceiverPsiGate(int roleClass /* T+0x4BD4[k] */) { return roleClass != 0; }
// GF mapping: roleClass==0 is the keeper → PsiGate ≡ !IsKeeper(r).
```
Unblocks: **OneTwo** receiver predicate P2 (its live closer). No new GF state needed: GF's keeper exclusion (`keepKeeper=false` / `KeeperSlot`) already expresses it.

## 4. `K+0x6E` in DHB (gate + copier CONFIRMED; ultimate writer OPEN)

### 4.1 K identity (CONFIRMED)

DHB asm `0x142129CC4 MOV RCX,[RSI+0x20]` — ctx+0x20 = ctx[4] = K = `BP+0x78` block (carrier-ai Unit context). Gate: `0x142129CB7 CALL 0x140A6E5E0` (first-time-while-receiving bit); `0x142129CBC TEST AL,AL; JNE exit`; `0x142129CC8 CMP [RCX+0x6E],AL` (AL==0 here); `0x142129CCB JE exit`. So **DHB requires `K+0x6E != 0`** (after the first-time bit is clear).

### 4.2 Copier (CONFIRMED raw)

`0x140AC0280` copies a byte row: `0x140AC035E MOVZX EAX,[RAX+0x90]`; `0x140AC0365 MOV [RBX+0xE6],AL`, null path `0x140AC0371 MOVB [RBX+0xE6],0`. `K+0x6E` relative to `K=BP+0x78` is `BP+0xE6`; so **`K+0x6E` is written by `0x140AC0280` from `*(param_1+0xC8)+0x90`**. Callers (E8 scan): `0x140ABF7FD`, `0x140AC0616`. What sets the ultimate `+0x90` byte and when the copier runs: OPEN.
Semantics INF (carrier-ai + goal-mouth/Safety uses): volley/header (first-time, ball-in-air) flag — cf. goal-mouth `K+0x6E!=0 → set` (`0x142119672–90` per image-rows §2.8) and Safety keeper gate `+0x6E==0 || height<1.5`.

### 4.3 Port statement / missing GF state

Missing GF state (exact): one carrier-kinematics byte — volley/header flag (`K+0x6E`, copier `0x140AC0280`, ultimate writer OPEN). `DHBGate = (flag!=0) && !FirstTimeWhileReceiving(state)`. Unblocks: **DHB** (its first gate). GF has no ball-flight/on-volley carrier flag; stand-in `false` keeps DHB silent by design.

## 5. BackSpace curve-gate input (COVERED by image-rows §2.3 — no new decode)

- BackSpace needs `0x1421225E0`: `k·mate.x >= k·curve(mate.z)` with frames = `int(0.0+0.5) = 0` — the `*0.0` is REAL (carrier-helpers §1.4 CONFIRMED asm `MULSS XMM0,XMM7`, XMM7=0). Reader `0x142139650`: `u=clamp(z·scale+35,0,70)`, lerp (lane-units2 §2.4b).
- Writer **is known**: `0x142138220(this=IA+0x2B0)` called from image builder at `0x142119468` (single E8 caller — CONFIRMED scan: `38220` has exactly 1 caller). Layout/CurveEval exact per image-rows §2.3; only the segment-construction step (dropped arg in `0x140470D00`/`0x140A8F7C0`) and sort tie rule are OPEN.
- Port statement (pure once the table exists): `CurveEval(c,z)` exact (image-rows §4). Missing GF state: the 71-sample `Curve` table rebuilt per tick (`BuildCurve` segment step OPEN). Unblocks: **BackSpace** corner/curve gate. One-touch G12 uses the same table.

## 6. GoalFront picker inner gates

GoalFront's lane is fixed (20 m/80°/1 m, carrier pos, heading-table θ — lane-structs §3.2); its picker reads the **same S1 family** (angle table `0x140A71260`, list row `0x140A714E0`, score row). No separate decode item: the picker unblocks with §1. Inner per-mate gates were not split by the probe (A4Lanes3 §5/§7 item 8) — next probe work, not a missing-input decode.

## 7. Corrections to existing docs (doc:line, old, new, evidence)

1. `pes-re-lane-units2.md` §7 S6: "snapshot `[0xD]` = `T(side)+0x4BD4[k]` (writer INF)" → writer CONFIRMED: `0x14063C9F0` store `0x14063CBEA/0x14063CC06`, meaning = role class (`pes-re-role-writer.md`). Evidence: raw asm §3.2.
2. `pes-re-lane-units2.md` §7 S4/O12 + `pes-re-lane-core.md` O12: level/mode readers/writers were OPEN → reader `0x140A40BD0`, `/100` comparator `0x140A60360`, setters `0x140A424E0`/`0x140A428B0`, sync copy `0x140416FBE–DD`, mode reader `0x141052E30` CONFIRMED raw. OPEN remainder: ultimate value source + level enum. Evidence: §2.
3. `pes-re-lane-units2.md` §9 item 1 + `pes-re-image-rows.md` §7 item 1: "decompile unchecked `0x140A79640` callers" → narrowed: all team-tick-callee callers checked (§1.5); remaining unchecked callers are outside the tick. MA load `*(this+0x508)` pinned to `0x140457070`; tick chain `0x140458940/0x140457568/0x1404575FE` pinned by E8 scan.
4. `pes-re-carrier-ai.md` Unit context: `ctx[4] = BP+0x78` (K); `K+0x6E` = `BP+0xE6`, written by copier `0x140AC0280` (callers `0x140ABF7FD`, `0x140AC0616`). Evidence: §4.
5. `pes-re-lane-units2.md` §6 S18 "`K.+0x6C/0x6E/0x6F` not re-traced" → `0x6E` traced to copier (§4); `0x6C/0x6F` still not traced.
6. `pes-re-teamctx.md` §2.4 / `pes-re-teamtick.md` §3: NearestList writer still OPEN, but reader pattern CONFIRMED as computed-base (`0x140633940`: `T18 + slot·200 + 0x22E4C`) — explains why displacement store scans miss the writer. Evidence: §1.5.

## 8. Open items (numbered, each with the settling experiment)

1. **S1 owner point + writer.** Scan the not-yet-decoded `0x140A79640` callers outside the team tick for a stack-built `(id,d2)` list copied through a computed base into `MA+0x228EC/0x22AD4/0x229B4/0x22B9C/0x22A7C/0x22C64`; else a read-only write watch on `MA+0x228EC` during one live kick. Blocks §1.4, Safety/G5/DHB-score/GoalFront exactness.
2. **`MA` allocator** (`this+0x508` before the tick). Write watch at match setup. Blocks the MA↔T18 identity proof.
3. **Level value enum** (what 200/400/500/600 select in-game) + `+0xA29C`-set conditions + `M+8` mode writer. Trace `0x14044A9D0←0x1404462AC` upward and the `0x141FBE…` setup callers of `0x140A424E0`. Blocks GF value mapping (gate itself is portable now).
4. **`K+0x6E` ultimate writer** (`+0x90` of `*(BPblk+0xC8)`) + copier cadence (`0x140ABF7FD` vs `0x140AC0616` contexts). Blocks DHB semantics (volley/header INF).
5. **Curve segment step** (`0x140470D00`/`0x140A8F7C0` dropped arg at `0x142138220`) + sort tie rule. Blocks `BuildCurve`; `CurveEval` already exact.
6. **GoalFront/BackSpace-validator/Safety-short inner gates** (probe §7 item 8) — next probe split, no decode blocker.
7. **`K+0x34` (S9), `K+0x6C`, `C[0xF]` (S18), mark count (S11), AI bytes (S12/S13), history (S17)**: named stand-ins, unchanged.

## 9. Method and budget

- Bridge: `decompile_function` (40BD0, 052E30, 60360, 63C560, 63C9F0, 714A0/C0/E0, 71260, 507740, 79640, 457070, 45B090, 45DF70, 45E0D0, 458940, 44AD40/44A9D0, FC7ED0, AC0320, 48A30 + 20 team-tick callees + 6 MA-chain fns + 6 helpers); `disassemble_function` (predicate ψ window); raw `objdump` for every CONFIRMED asm claim.
- Scans (all on `pes_full.bin`): E8-caller scans (714A0 58, 714C0 20, 714E0 110, 71260 80, 507740 41, 79640 73, 38220 1, 457070/5B090/E0D0 1 each, 40BD0 43, 052E30 101, 424E0 9, 428B0 1, AC0280 2); disp-occurrence scans (A294 10, A29C 11, 4BD4 59 — all reads, no direct store — confirming the role-writer doc; 22E4C 571, 228EC 14, 22AD4 3, 22B9C/229B4 15, 22A7C/22C64 2, 22CBC 8, 22D84 38).
- Not run: no builds/tests (decode task), no live PES, no commits. ~110 requests used.
