# PES 2021: callers of the foul/offside stoppage functions, the A1 record, the global, and the GF counterparts (SanctionCaller)

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000). Live: read-only snapshot reads of the existing rendezvous captures under `<workdrive>/gf-Rendezvous-scratch/live/` (no new captures, no signals, no writes to PES). The Ghidra bridge (127.0.0.1:8080) was DOWN this pass (HTTP 000), so every decode is capstone on the image plus `.pdata` bounds, plus the captures. Scratch: `<workdrive>/gf-SanctionCaller-scratch/` (`pdis.py`, `pdata.py`, `xscan.py`, `reach.py`, `snap.py`, `dump_ref.py`, `decode_calls.py`, `flow_a1.py`). Labels: CONFIRMED = image disassembly, a call-graph scan, or a live snapshot this pass; INF = inference; OPEN = blocker named. No PES bytes quoted. No float constants quoted.

Sizes (`.pdata`, script): 0x14047F170 = 2771 B; 0x140480910 = 707 B; 0x14047E940 = 193 B; 0x14047FEB0 = 374 B; 0x140446960 = 446 B; 0x140A3A690 = 468 B; 0x140A3A930 = 338 B; 0x140A39A80 = 434 B.

---

## 0. Summary

1. **Who calls the sanction tree.** `0x14047F170` has one caller, `0x14047E940`. That has one caller, the foul builder `0x140480910`. The builder has one caller, the wrapper `0x14047FEB0`. The wrapper has three callers, all in the step programme (switch `0x140446390`, reached from the runner `0x14044C880`):
   - `0x140446960`, step 5, foul stop (p5 = 0, p6 = 0). The sanction call runs on sub-state 0 only, so the decision is made **once, on the first frame of the stop**.
   - `0x140446603`, p5 = 1 (the late-card step). The doc `pes-re-foul-choreography.md` §1.1 calls it step 6 at `0x140446520`; the function that holds the call is `0x140446603` (INF on the step label).
   - `0x1404467F8`, p6 = 1 (the injury step). The doc calls it step 9 at `0x1404466F0` (INF on the label).
2. **The tree does not build A1.** `A1 = *(*(Flow+0x17860))` (CONFIRMED, all three callers). `Flow+0x17860` points at a 16-byte pair `{owner+0x228, owner+0x18}` that a wrapper builds on its own stack and publishes just before it calls the step driver. So `A1 = owner+0x228`: an analyze-side sub-object of the owner, alive only while the driver runs. Its vtable is `0x14259D590` (CONFIRMED live; named ANALYZE in `pes-re-s7-records.md`, INF on identity).
3. **The global** that `0x141F500D0` stores is `0x1437F5D50`, the fixdemo DB singleton (the load and the store hit the same VA; CONFIRMED). Within the decision's direct-call closure it is unreachable; only the presentation builder `0x14047C500` calls the getter. **Stub decision:** stub the builder's presentation, not the DB.
4. **Involvement callers.** `0x140A3A690` has two call sites: the judge `0x140A3A930` (at `0x140A3A9B6`) and the restart command `0x140A39A80` (at `0x140A39AF6` and `0x140A39B6C`) (CONFIRMED). Both are vtable actions run by the per-official dispatcher `0x1405141F0`. The argument `A0` is the official's per-frame record, and `A0+4` is the official index. Live captures show 22, 23 and 24 (CONFIRMED).
5. **Live confirmation is partial.** Some 40 involvement and action captures exist, all pre-match (period 0, play state 2 or 3, restart kind 1). The sanction tree never fired in any capture, although its hooks (`foul_sanction_47F170`, `foul_sanction_caller_480910`) are in `hooks_live2.cfg`. No capture has a foul stop, so the foul-field meanings are unverified against a live foul.

---

## 1. The chain: foul sanction (CONFIRMED unless marked)

| Step | VA | What it does | Evidence |
|---|---|---|---|
| driver | `0x140445E90` | calls the runner (its ret `0x1404462A1` is the hook's caller) | captures `restart_44C880` |
| runner | `0x14044C880` | situation-sequencer runner; calls the switch `0x140446390` (ret `0x14044C9AD` for `restart_446390`) | captures seq012/016/029–036, seq067–072 |
| switch | `0x140446390` | dispatches on step id (rdx = 3 and 0xB seen live) | captures seq067–072 |
| step 5 | `0x140446960` | foul stop; sub-state 0 (`[Flow+0x8F2] == 0`, `0x14044699E`–`0x1404469A7`) calls `0x14047D5D0` (reset), then `0x14047FEB0` at `0x140446A7F` | disasm |
| p5 = 1 step | `0x140446603` (call at `0x140446689`) | late card | disasm |
| p6 = 1 step | `0x1404467F8` (call at `0x1404468EB`) | injury | disasm |
| wrapper | `0x14047FEB0` (call at `0x14047FF06`) | builds the scene request and forwards to the builder | disasm |
| foul builder | `0x140480910` | scene id → `req+0`; decision via `0x14047E940` at `0x140480A1C`; FixDemo build `0x14047C500` at `0x140480B98` | disasm |
| out-vector | `0x14047E940` (call at `0x14047E977`) | zeroes the out vector; calls the tree with `rdx = A1` | disasm |
| sanction tree | `0x14047F170` | returns the scene id in eax | disasm |

**Arguments at each level (CONFIRMED):**

| Level | rcx | rdx | r8 | r9 | Stack |
|---|---|---|---|---|---|
| step 5 → wrapper (`0x140446A7F`) | `req = Flow+0x6D8` (`lea` at `0x140446A68`) | `A1 = *(*(Flow+0x17860))` (`0x140446A25`, `0x140446A33`, `0x140446A7C`) | `Flow+0x2790` (`0x140446A59`) | `Flow` (`mov r9,rbx`, `0x140446A74`) | p5 = 0, p6 = 0, p7 = 0xFF |
| p5 = 1 step → wrapper (`0x140446689`) | `Flow+0x6D8` | same A1 form (`0x14044665C`, `0x140446686`) | `Flow+0x2790` | `Flow` | p5 = 1, p6 = 0, p7 = 0xFF |
| p6 = 1 step → wrapper (`0x1404468EB`) | `Flow+0x6D8` | same A1 form, via `rsi` (`0x1404468C2`, `0x1404468E8`) | `Flow+0x2790` | `Flow` | p5 = 0, p6 = 1, p7 = 0xFF |
| wrapper → builder (`0x14047FF06`) | `req` (unchanged) | caller's rdx (= A1) | caller's r8 | caller's r9 (`Flow`) | p5, p6 forwarded |
| builder → out-vector (`0x140480A1C`) | `req` | `&out` (`lea rdx,[rbp+7]`, `0x140480959`) | **builder's rdx = A1** (`mov r8,rdx`, `0x140480956`) | `Flow` | — |
| out-vector → tree (`0x14047E977`) | `req` (A0) | **A1** (`mov rdx,r8`, `0x14047E974`) | — | — | 5th = 1 |

**Builder outputs (CONFIRMED):** `[req+0] = scene id` (`0x140480A8D`); `[Flow+0] = id` (`0x140480B6F`); the cut-list counter `[Flow+0xC]` is incremented (`0x140480AF3`). The foul event descriptor `{side = MI+0x1558, pos = 0, player = 0xFF}` is built by `0x14047C1D0` (`0x140480B50`). The no-scene id `0x20022` returns early (`0x140480A90`).

**Wrapper outputs (CONFIRMED):** `[req+8]` (the start gate, a frame count) is written at `0x14047FF35` and `0x14047FF6C`.

**Tree prologue (CONFIRMED):** `r12 = A1` (`0x14047F1B0`); `[rsp+0x30] = A0 = req` (`0x14047F1B3`); `G = [0x1436F3DC8]` (`0x14047F1BD`); `r13 = MI = G+0x270` (`0x14047F1C4`); `r14 = G+0x2AA8` (`0x14047F1CB`); `rsi = [A1+0xE0]` (`0x14047F1D2`); the victim index `MI+0x1594` is read at `0x14047F1F9` and must be ≤ 0x15.

---

## 2. A1 — the input record of the tree

### 2.1 Where A1 comes from (CONFIRMED)

- **Writer 1, zeroing:** the Flow constructor (function `0x140441490`) zeroes `Flow+0x17858` and `Flow+0x17860` (`0x1404414E0`).
- **Writer 2, the publisher:** `0x140444000` stores its `r8` into `Flow+0x17860` (`0x140444009`). It has four call sites, in four functions `0x140421DC4`, `0x140422047`, `0x14042334B` and `0x140423B04` (call sites `0x140421DF6`, `0x14042208B`, `0x14042337D`, `0x140423B36`). Each function does:
  1. `lea rdx,[owner+0x228]` and `lea r8,[owner+0x18]` (e.g. `0x140421DC9`, `0x140421DC0`);
  2. `call 0x140440D50` with `rcx = &slot`, a stack slot. The callee stores `[slot] = owner+0x228` and `[slot+8] = owner+0x18` (`0x140440D50`, `0x140440D56`);
  3. `call 0x140440E60` with `rcx = owner+0x6E0`;
  4. `call 0x140444000` with `rcx = [owner+0x38]` (= Flow), `rdx = owner+0x6E0`, `r8 = &slot` (`0x140421DF6`);
  5. `call 0x140446310` (the step driver, `0x140421DFF`), then `call 0x140445E70`.
- So `Flow+0x17860 = &slot`, a stack address that lasts for the driver call, and `A1 = *slot = owner+0x228`.
- **Live (CONFIRMED, seq012 and seq067; read-only):** `[Flow+0x17860]` holds a thread-stack address (`0x1C75FC60`), and its first qword is the heap object `0x34CEC318`, whose qword 0 is `0x14259D590` (a vtable). The Flow used was the runner's `rcx` (`0x34CEF750`).

**Owner:** the object whose `+0x38` is Flow. Its class and constructor are OPEN (not traced this pass).

### 2.2 Fields the tree reads (CONFIRMED by disassembly unless marked; the harness spec `14047F170.json` on branch `FoulOffside` lists the same 53 inputs)

| A1 offset | Read where | Meaning | Label |
|---|---|---|---|
| +0x00 | (vtable, live `0x14259D590`) | polymorphic object; ANALYZE lane per `pes-re-s7-records.md` | CONFIRMED vtable; identity INF |
| +0x18 | `0x14047F6DF`: `rcx = [A1+0x18]`; `add rcx,0x24A0C` (`0x14047F6F2`); `call 0x14042D5E0` (`0x14047F701`) with `r8 = r15` (the victim or fouler index) and `rdx = &out` | pointer to a block; the block's `+0x24A0C` holds per-slot records, and the tree tests the out's dword `+8` `== 3` (`0x14047F70D`) | pointer and offsets CONFIRMED; `0x14042D5E0` has no `.pdata` entry; identity OPEN |
| +0x6C + 12·k (k < 40); header dword at +0 | `0x14047F757`: `rcx = A1`; `call 0x140A81230` (`0x14047F75A`) | the callee compares `[A1]` (dword) with its `edx`, then scans the 40-entry, 12-byte table from `A1+0x6C` and returns the entry address, or `A1+0x24C` when not found | layout CONFIRMED from disassembly (`0x140A81570`–`0x140A81592`); meaning INF (event-slot lookup) |
| +0xE0 | `0x14047F1D2` (`rsi`), `0x14047F357` | pointer to the match-history object (see 2.3) | CONFIRMED |
| (none directly) | `0x14047F78E`: `rcx = A1`; `call 0x140A81040` (`0x14047F791`) | **dead argument:** `0x140A81040` never reads `rcx` (it reloads G and reads MI itself) | CONFIRMED |

### 2.3 matchHist (A1+0xE0) — the foul event ring (CONFIRMED)

`0x140A3E150(rcx = E0, edx = kind k, r8 = i)` returns the i-th newest record of kind `k`:
- kind stride `0x1908` (`0x140A3E160`: `rdx = E0 + k·0x1908`);
- count `dword [kind+0x1900]` (`0x140A3E16A`); if `count <= i` it returns a static sentinel (`0x140A3E155`);
- head `dword [kind+0x1904]` (`0x140A3E173`); slot `(head − i) mod 8`;
- record = `kind + slot·0x320` (800 bytes; `0x140A3E183`).

The tree calls it with `k = 4` (`lea edx,[r8+4]`, `r8 = 0`; `0x14047F24D`, `0x14047F35F`), so E0's kind-4 ring is the foul event history, and the newest record is read. The tree tests that record with `dword [rec+0] == 0x37` and `byte [rec+0x42] != 0` (the injury veto, `0x14047F364`–`0x14047F372`). Other record fields (`+0x18` foul kind, `+0x1C` card colour, `+0x20`/`+0x28` player ids, `+0x43` PK flag) come from `pes-re-foul-choreography.md` §1.2 (OBS in that doc; not re-decoded here).

`E0+0x1900` and `E0+0x1904` (spec inputs) are the kind-0 block's count and head, by the accessor's stride. The tree's own body does not read them; the closure reads them through `0x140A3E150`. Live: `E0+0x1900 = 8` in seq067 (kind-0 count), 0 in seq012 (CONFIRMED read).

### 2.4 Field list for the GF input map (A1)

| Field | Type | GF counterpart | Status |
|---|---|---|---|
| A1 header dword; table `A1+0x6C` (40 × 12 B) | event-slot table | none found | OPEN (meaning) |
| A1+0x18 → `+0x24A0C` per-slot record | per-player state | none found | OPEN (`0x14042D5E0` not decoded) |
| A1+0xE0 kind-4 ring (800 B records) | foul history | `MatchData` events (`matchdata.hpp:529`), `Player::GetCards()` (`player.hpp:213`) | partial |

---

## 3. The sanction request A0 (= req, Flow+0x6D8)

- Written by the builder: `[req+0] = scene id` (CONFIRMED); the wrapper writes `[req+8]` (the start gate: `gameSpeed·3/speedRate`, ×6 for injury, per doc §9.2; CONFIRMED write, value INF from doc) and `[req+0xC]` (the start flag, doc §9.2).
- Read by the tree: `byte [req+0xD] >= 1` (`0x14047F743`) is the EXCITE-played counter (doc §1.1).
- GF counterpart: none. GF has no scene request. Its foul hold is `kPesHoldFoul` (5.0 s) via `ScheduleStop` (`referee.cpp`, `CheckFoul`).

---

## 4. Global `0x1437F5D50` (the stored pointer)

- `0x141F500D0` (CONFIRMED disasm): `mov rax,[rip+0x18A5C6C]` at `0x141F500DD` and `mov [rip+0x18A5C33],rax` at `0x141F50116`. Both target **`0x1437F5D50`**.
- Body: if the global is null, allocate `0x52E0` bytes via `0x1403FB410` with a type tag `0x19`, construct via `0x141F4C950`, and store the pointer to the global. It is the `fixdemo::DB` singleton (`pes-re-cutscene-roles.md`, OBS).
- **Callers:** 195 direct E8 sites image-wide (scan). In this chain, the five sites `0x14047C53F`, `0x14047C552`, `0x14047C561`, `0x14047C58A`, `0x14047C5A0` lie inside the FixDemoMatch builder `0x14047C500` (`0x14047C500`–`0x14047C69B`, pdata). Of the 88 distinct caller functions, only `0x14047C500` is in this chain. The builder is called only by `0x140480910`, at `0x140480B98`, after the scene id is decided.
- **Transitive check (CONFIRMED by `reach.py`):** the direct-call closure from the decision roots (`0x14047F170`, `0x14047E940`, `0x140480910` with its `0x14047C500` edge removed, `0x14047EA10`, `0x14047E060`) visits 32 functions and never reaches `0x141F500D0`. Three indirect call sites in that closure were not followed (OPEN: name their targets). Control: `0x14047C500` reaches the getter directly.

**Stub decision (for Main):** stub the FixDemo match builder `0x14047C500` with a named `// ponytail:` stand-in (no presentation), and keep `0x14047F170`, `0x14047E940` and `0x140480910` up to `0x140480B98` as the decision. The DB singleton is not needed by any GF path. Blocked only on the 3 indirect sites (item 8).

---

## 5. Callers of the officials' involvement `0x140A3A690`

### 5.1 Call sites (CONFIRMED by E8 scan + pdata)

| Call site | Enclosing function (pdata) | Gate before the call | Argument |
|---|---|---|---|
| `0x140A3A9B6` | judge / approach `0x140A3A930` (338 B) | `cmp byte [rdi+0x30],0` (`0x140A3A9AD`), `jne 0x140A3AA26` (`0x140A3A9B1`); `rdi` = judge's `rcx` | `rcx = rbp` = judge's `rdx` (`0x140A3A9B3`) |
| `0x140A39AF6` | restart command `0x140A39A80` (434 B) | after the card-latch branch on `dword [rdi+0x1554] == 5` (`0x140A39ADC`–`0x140A39AEB`) | `rcx = rsi` (`0x140A39AF3`) |
| `0x140A39B6C` | restart command `0x140A39A80` | after the anime-class test via `0x140A6E3F0` (`0x140A39B4C`–`0x140A39B57`) | `rcx = rsi` (`0x140A39B69`) |

The judge, latch and restart are reached through the dispatcher `0x1405141F0` by vtable (returns `0x140518EB8` in `0x140518E20`; `0x140518DFF` in `0x140518DD0`; `0x140514181` in `0x1405140E0`). The dispatcher's direct callers (CONFIRMED E8 scan): `0x140930A0D`, `0x14094E34F`, `0x140961FE7`, `0x1409649A9`, `0x1409CC10C`, and `0x140A39C0B` (inside the restart command).

### 5.2 `this` and arguments (CONFIRMED: live)

The involvement's `A0` = `rcx` = the official's per-frame record (`[A0+4]` = index). Captured (`call.bin` decoded, snapshot read; `[A0+0x30]` read as a byte for the judge gate and as a dword for the restart state):

| Capture | Hook | `rcx` (A0 or action) | `[rcx+4]` | `[rcx+0x30]` | `rdx` | Ret |
|---|---|---|---|---|---|---|
| seq039 | involve | `0x3342DDD0` | 22 | byte 0 | 0x16 | `0x140A3A9BB` (judge) |
| seq042 | involve | `0x325E2800` | 23 | byte 0 | 0x17 | `0x140A3A9BB` |
| seq046 | involve | `0x32BF4740` | 24 | byte 0 | 0x18 | `0x140A3A9BB` |
| seq059 | involve | `0x3342DDD0` | 22 | byte 0 | (garbage) | `0x140A39AFB` (restart) |
| seq038 | judge | action `0x3342DEC8` (= record+0xF8) | — | byte 0 | `0x3342DDD0` (`[+4]` = 22) | — |
| seq058 | restart | action `0x3342DE88` (= record+0xB8) | — | dword 9 | `0x3342DDD0` | — |

The judge's `rdx` is the same official record, so the involvement's `A0` is the **judge's `param_2`**, and the judge is gated on the action object's byte `+0x30`. Action objects per official (captures): official 22: judge/latch at +0xF8, restart at +0xB8; official 23: `0x325E2898` (+0x98) and `0x325E2858` (+0x58); official 24: `0x32BF47D8` (+0x98) and `0x32BF4798` (+0x58). Index 25 was never captured (OPEN).

Which PES official is which index: INF. The vtable map (`pes-re-ref-record.md` §5.4) puts the referee on C (judge + latch), the assistant on A (look-at) and the fourth on B (restart). These captures show 22, 23 and 24 all running judge and latch, so the index→role mapping is OPEN.

### 5.3 When

- Per frame, while the stoppage is armed. The captures are at play state 2 (stopped) or 3 (set-play ready), so the involvement runs on each frame the judge or restart is dispatched for a stop (CONFIRMED live, seq037–066).
- The judge calls it only when the official's action byte is 0 (CONFIRMED). Its result (`al`) drives the latch branch that follows (`0x140A3A9BB`).

---

## 6. Live evidence and what is still unverified

- **Involvement and action captures** (seq037–066, launch pid 3652091): all pre-match (`period = 0`, `playState` 2 or 3, `restartKind = 1`, `stopReason = 0`, `foulKind = 0`, fouler and victim = 0xFF). They confirm `A0`, the indices, and the judge gate. They do not show a foul.
- **Sanction tree and builder:** installed in `hooks_live2.cfg` (`foul_sanction_47F170`, `foul_sanction_caller_480910`), but **no capture**. The step captures (`restart_446390`, seq067–072) show step 3 and 0xB only, with `stopReason = 4` (ball out). **No capture has `stopReason = 5`.**
- Old launch (pid 2802803, seq012–027): `restartKind` 1 and 3, `stopReason` 0, 3, 4. None is a foul.
- **OPEN:** the value of every A1 and E0 field at a foul stop. A capture at a foul stop (`MI+0x1574 == 5`) would confirm the foul-kind, card-level and fouler and victim meanings.

---

## 7. GF counterparts and the input map

### 7.1 Foul sanction (replaces: the scene, the no-scene hold, and the card's timing)

GF code (`src/sim/onthepitch/`):
- `referee.cpp` `JudgeContact` (line 1007): PES points → level → `FOUL_TYPE_FOR_LEVEL = {0,1,1,2,3}`. Level 2 ("foul + warning") folds into 1: a lossy map (CONFIRMED by reading).
- `referee.cpp` `RecordFoul` (1062): sets `foul.foulType`, `foulPlayer`, `foulVictim`, `foulTime`, `foulPosition`.
- `referee.cpp` `CheckFoul` (1112): advantage (`pes::ShouldPlayAdvantage`), the whistle hold (`k.whistleHold_s`), `StopPlay`, `ScheduleStop(kPesHoldFoul)`, `buffer.restartPos = foul.foulPosition` (or the penalty spot), the cards given at the whistle (`GiveYellowCard`/`GiveRedCard`, lines 1195–1198), `AddCard` (1203), `AddEvent` (1207).
- `referee.cpp` `IssueDeferredCards` (743) and `cardbook.hpp/.cpp`: advantage-eaten cards, shown at the next stop.
- `referee.hpp:53` `Foul` struct; ladder 0 none / 1 foul / 2 yellow / 3 red.

**PES → GF input map (sanction):**

| PES input | PES source (this report) | GF counterpart | Status |
|---|---|---|---|
| `MI+0x1168` period | MI | `PesClock` `period` (`pesclock.hpp:95`) | 1:1 |
| `MI+0x1554` restart kind (6 = PK) | MI | `restartKind` (`pesevents.hpp:488`) | 1:1 |
| `MI+0x155C` / `MI+0x1564` spot | MI | `Referee::buffer.restartPos` (= `foul.foulPosition`, or the penalty spot) | 1:1 once set |
| `MI+0x158C` card level 0–4 | MI, written upstream by the PES judge (writer OPEN) | `foul.foulType` 0–3 (`FOUL_TYPE_FOR_LEVEL`); level 2 (foul + warning) folds into 1 | partial (no warning level) |
| `MI+0x1588` foul kind (3 = indirect → no scene; 1 = offside) | MI | none (GF `Foul` has no direct/indirect; offside is `pendingOffside`) | none |
| `MI+0x1590` fouler / `MI+0x1594` victim (0..21, 0xFF) | MI | `foul.foulPlayer` / `foul.foulVictim` (`Player*`) — needs a PES slot index per `Player` | 1:1 via an index map (not built) |
| `MI+0x15B8` advantage-card flag (meaning INF) | MI | `foul.advantage` / `CardBook` deferral | partial, OPEN |
| `MI+0x1574` stop reason 5 | MI | `buffer.active` + `ScheduleStop` | 1:1 in effect |
| `G+0x1928+0x254` team A attack dir; `G+0x1978` team B | G | `Team::GetSide()` | 1:1 (team B: binding to check) |
| `G+0x2AA8` field info (goal-area half-width) | G | `pesPenaltyAreaHalfWidth` / FieldInfo | 1:1 |
| `A0 = req` (scene id, start gate, EXCITE counter) | Flow | none — GF has no scene | none; stub |
| `A1+0xE0` kind-4 ring (newest foul event) | matchHist | `MatchData` events; `Player::GetCards()` (`player.hpp:213`) | partial |
| `A1+0x18` per-slot record; `A1+0x6C` table | A1 | none | OPEN |

**Timing difference (CONFIRMED by reading both sides):** GF gives and records the card **at the whistle** (`CheckFoul`, lines 1195–1203, the same tick it stops play). PES decides the scene **at the first frame of step 5** (sub-state 0), from the kind-4 record written when the match goes out of play with reason 5 (`pes-re-foul-choreography.md` §1.2, OBS). The card level it reads (`MI+0x158C`) is written upstream by the PES judge, whose writer is OPEN. So a switch to `0x14047F170` replaces GF's **scene/no-scene and hold** (GF has none) and only part of the card path. The card decision itself (`JudgeContact` → `CheckFoul`) stays GF until the PES writer of `MI+0x158C` is decoded.

### 7.2 Officials' involvement (replaces: the approach gate and the linesman hold; GF has no boolean for it)

GF code:
- `player/controller/refereecontroller.cpp` `RequestCommand` (from line 62). Referee case: the approach gate `buffer.active && (foulType == 2 || 3) && prepareTime > now + 5000` (lines 64–68). Below it, the approach distance is taken from `judgeApproachSq_m2`; otherwise the chief spot (`chiefSpot`), or the NORMAL force-field movement.
- `refereecontroller.cpp` linesman case (from line 160): `holdForStoppage = !match->IsInPlay()` (line 180). The offside line is chased only in play.
- `officials.hpp`: `Officials` holds `referee` and `linesmen[2]`. There is no fourth official.

**PES → GF input map (involvement, `0x140A3A690`):**

| PES input | PES source | GF counterpart | Status |
|---|---|---|---|
| `A0+4` official index (22–25) | official record | `PlayerOfficial` (referee, `linesmen[0..1]`) — **no fourth official** | index→role INF; fourth official missing |
| `MI+0x116C` play state (1 in play, 2 stopped, 3 set-play ready) | MI | `match->IsInPlay()` (1 vs 2); `buffer.prepared` / `started` (3) | partial |
| `MI+0x11B2` armed flag (set with 116C == 3) | MI | none (INF: `buffer.prepared`) | none |
| `MI+0x1544` / `+0x1548` / `+0x154C` offside words | MI | `offsidePlayers` map; `pendingOffside` | partial (the doc §4.1 gap stands) |
| `MI+0x1554` restart kind; `+0x1558` side; `+0x155C` / `+0x1564` spot | MI | `buffer.desiredSetPiece`, `buffer.teamID`, `buffer.restartPos` | 1:1 |
| `MI+0x1574`, `+0x1588` | MI | `buffer.active`, `foul.foulType`, `pendingOffside` | 1:1 / partial |
| `G+0x1928+0x254` attack dir | G | `Team::GetSide()` | 1:1 |
| `anime+1` class byte (0x5A goalkeeper, 0x5C) | G+0x19C8 | `pesroles` (different vocabulary) | partial |
| `man+0x55C` / `+0x564` position | G+0x1DD8 | `Player::GetPosition()` (PES x = GF x; PES z = GF y) | 1:1 |
| the action object's byte `+0x30` (judge gate) and `dword +0x1554 == 5` (restart gate) | per official | none | none |

---

## 8. Open items (numbered)

1. **A capture at a foul stop** (`MI+0x1574 == 5`, sanction hooks armed). Blocks confirmation of every foul-field meaning in §2 and §7.1. Ask the owner for a foul in the running match, or wait for the bot to produce one.
2. **Builder and owner of A1:** the class and constructor of `owner` (`A1 = owner+0x228`), and which of the four wrappers runs for which match phase. Blocks a full A1 layout.
3. **`0x14042D5E0`**, the per-slot accessor on `A1+0x18`: no `.pdata` entry; decode by hand. Blocks the A1+0x18 meaning.
4. **`0x140A81230`**, the A1 table lookup: its key dword and the `(kind, player)` meaning (INF only).
5. **MI+0x158C writer** (card level) and **MI+0x15B8** meaning. Block GF's card-level mapping (level 2 vs 1).
6. **Index→role** for 22–25 (captures show 22, 23, 24; 25 never captured). Blocks the per-official record.
7. **Bridge:** the Ghidra bridge was down this pass. Re-check the `.pdata` function boundaries with the bridge when it is back.
8. **Three indirect call sites** in the decision's direct-call closure (`reach.py` counted 3; not followed). Name their targets before the stub in §4 is final.
9. **Hooks:** the sanction hooks never fired. Check the hook-install step for `47F170`/`480910` on the next launch.
