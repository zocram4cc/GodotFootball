# PES type-13 record builder, transient bindings, emulated advance, writers (Type13, 26-10-26)

Read-only RE. Image `pes_full.bin` (VA offset 0x140000000). No PES bytes quoted.
Labels: **CONFIRMED** = disassembly/live read/emu run this pass; **INF** = inference; **OPEN** = blocker named.
Scratch: `<workdrive>/gf-Type13-scratch/` (harnesses + `s0`/`s1` snapshots + `pauses.jsonl` + `rapid.jsonl` + `watch.jsonl`).
Snapshots are PES-derived, never committed. Two full SIGSTOP snapshots taken this pass (3.99 s, 4.73 s pauses, both CONTed, logged).

Prior docs re-checked: `docs/re/pes-re-records.md` (Records), `docs/re/pes-re-advance.md` (Advance), `docs/re/pes-re-integration.md` (plan).

## 0. Headline verdicts

| # | Ticket item | Verdict |
|---|---|---|
| 1 | Type-13 builder among the 6 producer call sites | **CONFIRMED**: none of the 6 builds type-13 directly. Type-13 is built by the detector-vtable builder **`0x140A15E80`** (vtable `0x142640BC0` slot 0), at two `mov edx,13` sites (`0x140A1613A`, `0x140A16352`) inside one function (`0x140A15860`…`0x140A166E1`), reached via the analyze-family produce loop `0x140A0D190 → 0x140A0E400 → [slot-vt+8]`. The 6 `0x140A3DC40` call sites build types 7/8 or delegate to slot vtables (§1). |
| 2 | Transient bindings catch (many SIGSTOPs until `[D+0xB600]`/analyze-obj/`MI+0x68`/`+0xE0` non-null) | **NOT CAUGHT — hit rate 0% with reason**: 40 rapid SIGSTOP polls + 2 full snapshots + 30 background watch polls, all `MI=0` (pre-match lobby). But the premise is corrected: `[D+0xB600]` detectors are **223/223 non-null even at rest** (they are persistent lobby objects, not transients); the true transients are `MI` itself and the analyze `+0x20` struct. The lobby never builds them. No mid-frame catch without a live match (§2). |
| 3 | Advance: type-13 → `0x140A128E4`, N frames, `MI+0x1198`, 3x deterministic | **NOT ACHIEVED — precise reason named**: with a builder-faithful forged record the emu now runs the type-13 body cleanly (`bad=None`) but exits at `0x140A1282C`: the roster gate `0x140A0E690` returns 9 > 0x15 because the resting roster has only slots 0–1 bound (`[entry+0x10]` = 0,1,-1…) while the record needs a matching slot value. The inc never fires without a live-match roster (§3). Determinism of what runs: the clean-exit path is repeatable (same reached-set across runs). |
| 4 | Writers (ball, S4 list) with VA + call stack | **PARTIAL**: frame counter + publish + positions restated with VAs (§4, CONFIRMED re-check). **S4 nearest-list writer: narrowed to two register-based store sites** (`0x140A04429` `mov [r12],r15d`, `0x140A04024` reader of the same computed base; INF). **Ball writer: still OPEN**, but the reader side is mapped (MA+0x2482C readers at `0x140A1E460/0x140A20903`; the writer uses a computed base, same phenomenon as S4). |
| 5 | Fallback: two-snapshot one-frame diff + store search | **BLOCKED by no live match**: `MI=0` in every sample this pass (bot between matches), so no frame pair exists to diff. The static half (register-based store search) is done (§4). |

## 1. The type-13 builder (CONFIRMED)

### 1.1 The 6 `0x140A3DC40` call sites build 7/8, not 13 (CONFIRMED disassembly)

Producer `0x140A3DC40` (pdata `0x140A3DC40–0x140A3E13D`) has 6 direct `E8` callers (byte-scan CONFIRMED, matches Records §1.2):
`0x140A0D4C8`, `0x140A0D5F8` (detector loops over `[D+0xB600]`, records built by `0x140440D50`+`0x140A3CDD0` then per-slot `vt+0x10`/`vt+0x8`),
`0x140A0E0D3`, `0x140A0E17F`, `0x140A0E2A6` (builds **type 7** via `0x140A3D060`: `[0]=7,[4]=r8d,[8]=frame`, `edx=7`),
`0x140A0E486` (loop `0x140A0E400`, per-slot `vt+8`).
Disassembly of each site's builder preamble shows `mov edx,7` (`0x140A0E28B`), `mov edx,8` (`0x140A0E06F`, `0x140A0E164`), or vtable delegation — **no `mov edx,13` at any of the 6 sites or their inline builders**.

### 1.2 The actual type-13 builder: `0x140A15E80` family (CONFIRMED disassembly + live vtable census)

Image-wide scan for `BA 0D000000` (`mov edx,13`): 394 hits, **5** in `0x140Axxxx`:
`0x140A1613A`, `0x140A16352` (both call `0x140A3D060` = the record builder), plus `0x140A47D5C`, `0x140A6CE10`-adjacent `0x140AA8A25`, `0x140AC1D85` (different subfamilies, not detector records — INF).
The two detector sites sit in **one function** spanning `0x140A15860` (prologue `push rbp/r13/r15`) through `0x140A166E1` (`ret`), which is vtable `0x142640BC0` slot 0 (`0x140A15E80` entry label; vtable ref at file-offset `0x2640BC0` = VA `0x142640BC0` CONFIRMED by qword scan).

Call sequence at each site (identical shape):
`mov edx,0xd; movss xmm0,[rdi+0x314]; mov rcx,r14; mov r9d,[r15+0x1198] (frame); mov r8d,[rdi+0x18];
mov [rsp+0x28],eax ([r15+0x1168]); movss [rsp+0x20],xmm0; call 0x140A3D060`
then `lea rcx,[r14+0x14]; call 0x140A156F0` (record assert: `[rcx]=2`), then `movups` copy of detector struct `rdi+[0x8..0xCC]` → record `r14+[0x20..0xE4]`, `[r14+0x18]=[rdi+0xD0]`,
`[r14+0x318]=[rdi+0xD4]`, `[r14+0x319]=[rdi+0xD5]`, **`[r14+0x31A]=0` then conditionally non-zero**, `[r14+0x31B]=[rdi+0xD7]`,
then a 10×14 `mov [rdx],eax` table fill from `[rdi+rax*4+0xD8]`, then `call 0x140A16930`.

What fills `[4]`, `[8]`, `[0x24]`, `[0x31A]` (CONFIRMED):
- `[0]=13` (`edx`), `[4]=[rdi+0x18]` (detector slot word, `r8d`), `[8]=[r15+0x1198]` (MI frame, `r9d`). So **`[8]-[4]` = frame − slot-word**.
- `[0x24]=[8]-[4]` is NOT written by the builder — it is the **consumer-side gate** (`0x140A1283F` `cmp [r14+0x24],edi` where `edi` is derived from the `0x140A0E690` return; Records' forge step sets it, the builder does not emit it).
- `[0x31A]`: site 1 (`0x140A1613A`, gate `[r15+0x116C]==2`): the `+0x31A` write on this path is **absent** — the first builder's tail (`0x140A16221…`) writes `+0x318/+0x319/+0x31B` but no `+0x31A` store (falls through with whatever `0x140A156F0` left: 0). Site 2 (`0x140A16352`): explicit `mov byte ptr [r14+0x31A],0` at `0x140A1645F`. **Both builders zero `+0x31A`; the non-zero value comes from the `+0x31B`/`+0xD7` path or a later set — the stale rec0 type-13 has `31A=1,31B=0`, i.e. set after build or by the `0x140A16930` tail (OPEN which store sets it to 1).**
- When in the frame: site 1 fires when `MI+0x116C==2` (play phase 2); site 2 fires when the slot record matches (`[rdi+0xC]==[rbx+0x18]`) after the `0x140A3E7D0`/`0x140A60530`/`0x140A6CE10` filter chain. Both run inside the detector produce loop (below), i.e. **after `0x140A22130` setup, before `0x140A13060` consume** in `0x140A0D190`'s `ecx==1` branch.

### 1.3 Produce chain to the builder (CONFIRMED)

`0x140426760` (analyze step 5: `rcx=[obj+0x18]`, `rdx=[rbx+0x20]`) → `0x140A0D190` (sole caller `0x14042677D`, CONFIRMED)
→ `ecx==1` branch: `0x140A20020/0x140A21850/0x140A0E940/0x140A32CF0/0x140A0E6D0/0x140A32B90` → `0x140A0E400` (D-holder + analyze struct)
→ `0x140A0E444` loop over `[D+0xB600+8·i]` (0xDF iterations): per slot `call 0x140A3CDD0`, then `call [rax+8]` (**slot-vt+8 = the builder for that detector kind**), `test al,al` → `call 0x140A3DC40` (produce).
The type-13 detector kind is the vtable-`0x142640BC0` family (slot 0 = `0x140A15E80`).
Live census (read-only pread, 223/223 slots, CONFIRMED): slots 24,25 use sibling tables `0x142640B40` (`+0=0x140A15650,+1=0x140A166F0`) and `0x142640B68`; slots 38…96 use `0x142640F38` (`+0=0x140A1A6E0,+1=0x140A1A4C0` = the **type-14 builder**: `mov edx,0xe` at `0x140A1A51E`, same `0x140A3D060` shape with `[rbp+0x18/0x14]` slot words); slots 135–138 use `0x1426410C0+`. The `0x142640BC0` kind (type-13) has **no live slot at rest** — its detector is itself transient (bound only during the relevant phase), consistent with the missing mid-frame catch (§2).

### 1.4 Sibling builders mapped (CONFIRMED, for the record)

- Type-7: `0x140A0E2A6` via `0x140A3D060` (`edx=7`, `[4]=r8d` slot word, `[8]=r9d`=frame). Seen live in the E ring (`[4]==[8]`, frame-valued words 43318–44599, s1 snapshot).
- Type-8: `0x140A0E06F`/`0x140A0E164` via `0x140A3D060` (`edx=8`).
- Type-9: `0x140A17815` via `0x140A3D060` (`edx=9` at `0x140A17977`; gated on `[rax+0x11B4]==0` i.e. `MI+0x11B4==0` and ball-record change `[rax+0x2798]!=[rsi+8]`). Seen live in the E ring (type 9, `[4]` 43912–43922, `[18]=0`, s1).
- Type-42 (`edx=0x2A` at `0x140A15EDF`), type-43 (`0x2B` at `0x140A15DF1`), type-25 (`0x19` at `0x140A15CAA`), type-26 (`0x1A` at `0x140A15BC4`), type-24 (`0x18` at `0x140A1673E`), type-7 (`edx=7` at `0x140A165D4`) — all in the same `0x140A15860` containing function, different phase/filter gates.

## 2. Transient bindings: 0% hit rate with corrected premise (CONFIRMED)

- 40 rapid SIGSTOP polls (STOP→pread 5 qwords→CONT, ~0.5–2.5 ms pauses each, `rapid.jsonl`) + 2 full snapshots (3.99 s, 4.73 s, `pauses.jsonl`, every STOP CONTed via trap) + 30 background watch polls (`watch.jsonl`): **all `MI=0`, `Ecnt` 0→2, frame none**. The bot is between matches (pre-match lobby): no match step is running, so no transient exists to catch. Tries: 72 polls; hits: 0.
- Correction to the ticket premise (CONFIRMED by live pread): `[D+0xB600+8·i]` are **223/223 non-null at rest** (persistent detector objects with stable vtables; `D+0x68`/`D+0xE0` hold small changing integers, not pointers). The true per-frame transients are `MI = *(G+0x270)` (0 at rest) and the analyze `+0x20` struct (`[analyzeObj+0x20]` = `rsi` of `0x140A0D190`, holder of `MI+0x68`/`+0xE0`). A mid-frame catch requires a live match, not more lobby polls.
- What the lobby *does* show (CONFIRMED, s1): `Ecnt=2/Ehead=2` with live type-7/type-9 records churning (frames 43318–44599 in the ring) — the detector produce loop runs in the lobby, but the type-13 kind's detector is unbound and no type-13 is produced.

## 3. Emulated advance (CONFIRMED traces)

Harness: `forge_emu.py` (s0 snapshot + LazyMap + P1/P2/P3 chain + import models + `0x1403FF430/0x1403FF490` return-`0x18000003` models + auto-ensure of faulting globals).
Record: the **real stale type-13 from rec0** (`[4]=35050,[8]=35160,[0x18]=1,[0x20]=3,[0x24]=1,[0x318]=1,[0x319]=1,[0x31A]=1,[0x31B]=0` — the only `31A!=0` body ever observed).

Result (CONFIRMED, repeats identically): `bad=None` (clean return), reached `{115A0:12, 1281A:12, 12824:9, 0E690:9}` — i.e. **the type-13 body is entered, `0x140A0E690` is called, returns 9 > 0x15, exit at `0x140A1282C`**. `ES+0x1198` unchanged, 0 game writes.
Why (CONFIRMED): the resting roster (`G+0x180`, 24×`0x74`) has entries `[+0x10]` = 0,1,-1×22 — the `0x140A0E690` scan `[base+k·0x74+0x10]==eax` (with `eax` = `0x14044E200` bit-scan return over the vcall `[rax+0x68]`) finds no match ≤ 0x15 in this state. Firing `0x140A128E4` needs (a) a live-match roster with the record's slot value bound, and (b) the `0x140A0E690`-input vcall resolving — both absent at rest. No N-frame run exists to compare; the clean-exit path is deterministic across reruns (same reached-set, same counters).

## 4. Writers (CONFIRMED re-check + narrowed INF)

- **Frame counter**: `0x140A128E4` (`inc [rsi+0x1198]`, `rsi` = E-side `E+0x180C60`; reached via `0x140A0D190+0x1CA → 0x140A13060 → 0x140A115A0 → idxbyte-5 case`; gated by `[rec+0x31A]!=0` at `0x140A128D4`). Sibling E-side writers same body: `0x140A12851` (`[+0x118C]+=edi`), `0x140A12857` (`[+0x1190]++`), `0x140A128DE` (`[+0x1194]+=edi`). Publish to MI: `0x14100CE6C` (`mov [rbx+0x1198],eax`, func `0x14100CCA0–0x14100CCC6`, called from `0x1410D97A6/0x1410D9942/0x14111025C/0x14113B2F8/0x14113B427/0x14113B5D7/0x1414E17B2`). The `0x1410D97A6` call site builds `(rcx = table+idx·0x11A8+0xE8, rdx = rbp-0x40)` in a loop over `[rbp+0x240]` entries — the publish source is a per-side block (`rbp`), not MI directly (CONFIRMED disassembly).
- **Positions** (`movss`, static scan re-checked): `man+0x55C`: `0x14168023A`, `0x141988598`, `0x14188CC8`-family (`0x141B88CC8` conditional write after `ucomiss [rcx+0x55C]`); `man+0x560`: `0x14168024A`, `0x1419885AF`, `0x141B88CD0`; `man+0x564`: `0x14168025A`, `0x1419885C6`; `man+0x578`: `0x140A85A49`, `0x140A85A83`. `man+0x574/+0x57C`: no `movss` store image-wide (loads only) — vector/computed stores, OPEN (same as Records).
- **S4 nearest-list writer (narrowed, INF)**: readers use `imul …,0xC8` + `lea …,[base+0x22E4C]` (23 LEA sites; in-analyze readers `0x140A25A94/0x140A26BFB/0x140A276C7`, consumer `0x140A17F4A`). The write side in the `0x140A03F80` containing function uses the computed base `rdx = [rsp+0x40]+rax` with `rax = r13·0xC8+0x22E4C` (`0x140A03FF6–0x140A0400F`): store `0x140A04429` (`mov [r12],r15d`, `r12` = list-slot pointer derived from that base) and the same-base reader `0x140A04024`. **INF: `0x140A04429` is the nearest-list id writer; call stack `0x140A0D190 → … → (containing fn of 0x140A03F80)` — the containing function's identity vs the impure `0x1404F8C70` region is OPEN (needs the `0x140A03F80`-region function-boundary + caller resolution).**
- **Ball writer (OPEN with reader map)**: no `movss`/`movups` store to `MA+0x2482C/+0x24834/+0x2601C` image-wide (CONFIRMED re-scan). Readers: `MA+0x2482C` at `0x140A1E460`/`0x140A20903`, carrier `+0x2601C` at `0x140A627EC/0x140A6302B`, grid `+0x26020` zeroed at `0x140A6F3DA` (`memset 0xC4E00` via `0x1415A1720`), ranks `+0x24710` no LEA in analyze range (computed-base writes). Same computed-base phenomenon as S4; the writer hunt continues from the `0x140A6F000–0x140A70000` MA-build region (entries from `0x140A5B319`, `0x140794517`; function-boundary mapping OPEN).

## 5. What blocks the match advancing in emulation (ranked)

1. A live match (MI non-null): the roster gate and all transient bindings exist only during a match step. 0/72 polls hit one.
2. The type-13 detector kind's binding (`0x142640BC0` slot, no live instance at rest) + which store sets `[rec+0x31A]=1` (both builders write 0; rec0's spent body has 1).
3. `0x14100CCA0` publish arc `(rcx table+idx·0x11A8+0xE8, rdx=rbp-0x40)` identity (loop at `0x1410D94F0`, per-side block `rbp`).
4. S4 writer function-boundary/call-stack proof (candidate `0x140A04429`, INF).
5. Ball-cluster writer (computed-base stores in the MA-build region).

## 6. Recommended next step

When a live match is up: rapid-SIGSTOP poll for `MI!=0`, then full-snapshot; rerun `forge_emu.py` with that snapshot's roster + the stale rec0 type-13 (or a fresh one from the live ring) — the `0x140A0E690<=0x15` gate is the first inc-firing gate. In parallel (static, no PES needed): resolve the containing function + caller of `0x140A04429` (S4 writer proof) and map the MA-build region's computed-base `movups`/`movss` stores (ball writer).
