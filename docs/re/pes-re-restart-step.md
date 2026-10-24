# PES restart step `MI+0x11B0`: container, writers, phase table, port spec (RestartStepRE, 24-10-26)

> **Main audit 23-10-26 (bytes):** `0x14042205E mov rsi, rcx` and the mirror `0x1404227F9 movups xmm0, [rsi+0x728]`: match. Correction: the phase-6 compare is the instruction at **`0x1405F5226`** (`80 BE B0 11 00 00 06 cmp byte [rsi+0x11B0], 6`); `0x1405F5225` is mid-instruction. The mutated immediate at `0x1405F522C` is correct.

Scope: decode `MI+0x11B0` (the restart-step byte) per the ticket: trace `FUN_140422030`'s callers
(vtable slot `0x142598558`) to name the `+0x728` container, sweep its writers, name phases
7 / 0xD / 0x10 / 0x13, confirm the named ones, record the restart order, and spec the GF port.
Read-only. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (offset = VA − 0x140000000).
Bridge `http://127.0.0.1:8080`, one request at a time. Live PES read-only (`sudo dd` on `/proc/2510857/mem`).

Base docs: `docs/re/pes-re-team-status.md` §4, `docs/re/pes-re-holder-leaves.md` §§1/5,
`docs/re/pes-re-A5Judge.md` §§2–4, `docs/re/pes-re-setpiece-choreography.md` §11.2,
`docs/re/pes-re-style-reverter.md` §§6.2/7.2.

Checker: `check_restart_step.py` (this scratch), GREEN 24-10-26 (37 rows); mutation
(`0x1405F522C` `06` → `07`) drives `E.cmp6` RED, reverted.

Floats: none quoted (no float immediates read this session). Every instruction below is
paired with the operand VA the disassembly shows at that instruction.

## 0. Script-read constants (checker output, pasted)

```
A.entry.rsi 488bf1 (0x14042205E MOV RSI,RCX)
A.mirror 0f108628070000 0f1183b0110000 (0x1404227F9/800 MOVUPS XMM0,[RSI+0x728] / MOVUPS [RBX+0x11B0],XMM0)
B.movzx 410fb687b0110000 (0x140458912 MOVZX EAX,byte [R15+0x11B0], R15=MI)
B.range 83c0f883f80e (ADD EAX,-8; CMP EAX,0xE; JA)
B.dl1 b201 (0x140458937 MOV DL,1)
B.dl0 33d2 (0x14045893B XOR EDX,EDX)
B.call.w1 e82b (0x140458940 CALL FUN_140457070)
B.tab 8/B/E/12->0x140458937; 14/15/16->0x14045893B; else 0x140458945 (15 RVAs at 0x140458960)
C.word1 66c783b01100000100 (0x1408AE747 MOV word [RBX+0x11B0],1)
C.dword1 c787b011000001000000 (0x140A11499 MOV dword [RDI+0x11B0],1)
D.set1 c683d827000001 (0x14044A319 MOV byte [RBX+0x27D8],1)
D.inc fe83d8270000 (0x14044A2DC INC byte [RBX+0x27D8])
D.cmp14 4180b8b011000014 (0x14044A2D2 CMP byte [R8+0x11B0],0x14)
E.cmp7 80bab011000007 (0x140926E58 CMP byte [RDX+0x11B0],7)
E.cmpD 80beb01100000d (0x140926D99+1 CMP byte [RSI+0x11B0],0x0D)
E.cmp10 4883bdb011000010 (0x1410D96C9 CMP qword [RBP+0x11B0],0x10)
E.cmp14.lane 80beb011000014 (0x1405CE57B+1 CMP byte [RSI+0x11B0],0x14)
E.cmp16 4180bdb011000016 (0x140929A51 CMP byte [R13+0x11B0],0x16)
E.cmp6 80beb011000006 (0x1405F5225+1 CMP byte [RSI+0x11B0],6)
E.movzx15 0fb68fb0110000 (0x14044BB63 MOVZX ECX,byte [RDI+0x11B0])
E.cmp15rel 83e915 (0x14044BB71 SUB ECX,0x15)
E.cmp16rel 83e901 (0x14044BB7A SUB ECX,1)
E.cmp13 80b8b011000013 (0x1404408FE+1 CMP byte [RAX+0x11B0],0x13)
E.cmp13b 80bfb011000013 (0x140440983+1 CMP byte [R15+0x11B0],0x13)
F.init0 89bb28070000 (0x1404F031D MOV dword [RBX+0x728],EDI)
F.init.edi0 33ff (0x1404EFACB XOR EDI,EDI)
F.set.r13 4d89ae28070000 (0x14041D19C MOV qword [R14+0x728],R13)
F.set.rax1 48898328070000 (0x14041B2D9 MOV qword [RBX+0x728],RAX)
F.set.rax2 48898628070000 (0x14041AA7A MOV qword [RSI+0x728],RAX)
F.set.rsi 4989b628070000 (0x140412541 MOV qword [R14+0x728],RSI)
```

## 1. Container: name and wiring — CONFIRMED structure, OPEN live instance

- `FUN_140422030` entry `0x14042205E MOV RSI,RCX`: the mirror source `RSI` is its own
  `param_1` (the `+0x728` container object O). `RBX` (the `MI` destination) is
  `FUN_140422CF0(param_1+0x98)` (decompile `d_422030.c` line 49; call at `0x1404220AE`).
- Mirror `0x1404227F9/0x140422800`: 16 bytes `O+0x728..0x737` → `MI+0x11B0..0x11BF`
  (checker rows `A.*`). The low byte `O+0x728` is the phase source each tick.
- O's class: primary vtable `0x1425984C0`, slot 19 (`0x1425984C0+0x98 = 0x142598558`)
  holds `FUN_140422030` (dump: `[0x142598558] = 0x140422030`; xrefs to the slot are DATA
  only — no direct `E8` caller image-wide: full 1,739,032-`E8` scan, zero hits). Secondary
  vtable `0x142598568` at `O+0x10` (installer `FUN_140420480`, called only from
  `FUN_140421610`; class size `0x3658`, `MOV EDX,0x3658` at `0x14041901F` in factory
  `FUN_140418940`, whose ctor call `0x140419038 CALL 0x14041F9A0` was verified by byte
  arithmetic). Slot-0 destructor `FUN_140421610`. (Cross-checked with
  `pes-re-style-reverter.md` §§6.2/7.2, which names the same class independently.)
- Per-tick driver: `FUN_140422030` line 108 calls driver `FUN_140445E90(*(O+0x38),…)`
  (A5Judge §3.1). Who calls slot `+0x98` per tick is OPEN: 201 `CALL [RAX+0x98]` sites
  exist (all classes' slot 19, not just O's); the sequencer-runner-adjacent
  `0x14044A6B3 CALL [RAX+0x98]` was eliminated — its object is `*(0x1434A2D40)` =
  `0x1434A2D48`-table whose slots `+0x88/+0x98` are `0x140A5E3B0/0x140A5D940` live,
  a different class. O's factory `FUN_14041A710` is recursive (sole `E8` caller is
  itself at `0x14041B23E`); its outer caller is OPEN.
- Live: no `0x3658`-class instance found this session (heap scan of all `1d/1e` rw ranges
  for primary `0x1425984C0` / secondary `0x142598568`: none; `G = 0x1EB942C0` carries a
  different vtable `[G] = 0x142599B58`). The owner's match (one outfield player/side)
  may not instantiate O, or O lives in an unscanned mmap region. OPEN.
- Two 16-byte copies of a live `MI+0x11B0` block were found once in heap
  (`0x1D4077B8 = MI+0x31B0`, `0x1D409B30 = MI+0x5528`, both inside the `MI` parent range)
  but read back zeros minutes later (match moved on); containers
  `0x1D407090/0x1D409408` read zero with `rc = 0` (mapped, value gone). INF only.

## 2. `+0x728` writers — CONFIRMED instructions, OPEN per-value mapping

Full-image disp32-`0x728` sweep: 484 hits, 110 code-range stores. Only two XMM sites touch
`+0x728` and both are READS (`0x1404227F9`, `0x1409767AB MOVSS`) — so the 16-byte block is
written by qword/dword pieces or memset-style init, never one XMM spill. No `C6/C7` imm
store to any `+0x728` exists image-wide (confirms the team-status triage).

| VA | instruction | function | value | status |
|---|---|---|---|---|
| `0x1404F031D` | `MOV dword [RBX+0x728],EDI` (EDI = 0, `XOR EDI,EDI` at `0x1404EFACB`) | `FUN_1404EFAB0` (ctor, called once from `FUN_1404F07A0+0x18AC`) | 0 (init) | CONFIRMED |
| `0x14041D19C` | `MOV qword [R14+0x728],R13` (after `CALL 0x140434650(old)`) | `FUN_14041D100` (region undefined in Ghidra; created via `dec.sh`, body `[14041d100,14041d132]+[14041d1b1,14041d1bf]`) | R13 (param) | CONFIRMED instr / OPEN value |
| `0x14041B2D9` | `MOV qword [RBX+0x728],RAX` (RAX = `FUN_140434360(string,1)` — a `0x77D8`-byte handle alloc, not a step number) | `FUN_14041B2xx` (RBX = the object passed to `FUN_14041A710` at `0x14041B23E`) | handle | CONFIRMED instr / OPEN value |
| `0x14041AA7A` | `MOV qword [RSI+0x728],RAX` (RSI = `param_1` of `FUN_14041A710`, `MOV RSI,RCX` at `0x14041A754`) | `FUN_14041A710` | handle | CONFIRMED instr / OPEN value |
| `0x140412541` | `MOV qword [R14+0x728],RSI` (ctor tail; neighbours `[R14+0x700]`, `+0x710/718/720` = RSI) | `FUN_1404123A0` | pointer param | CONFIRMED instr / OPEN value |

The `434360`-family setters store heap handles, and the init stores 0 — none writes a step
constant `6/7/8/…` as an immediate. The function that writes EACH restart-step value is
therefore still OPEN: the per-value writer is a register move whose value flow (which step
number reaches which setter on which stop) was not traced. The Flow-side step counter
`Flow+0x27D8` (`0x14044A319 =1`, `0x14044A2DC ++`, step `0x14` window, §3) is a separate
counter, not `O+0x728` (zero `+0x728` sites in `0x140440000–0x14046A000`).

## 3. Phase table — names from the step handlers that consume each value

Match-side placement switch `FUN_140926B00` (decompile `d_926b00.c`, bridge): switches on
`MI+0x11B0` with cases `1 / 5,7,9 / 10 / 0xC,0xF / 0xD,0x10 / 0x11 / 0x14,0x15,0x16`,
default `*(unit+0x7C9) = 0`. Values `6/8/0xB/0xE/0x12/0x13` never reach it (Flow-side or W1
only). W1 switch (`FUN_140458880`, checker `B.*`): `8/0xB/0xE/0x12` arm-and-run,
`0x14/0x15/0x16` run-if-latched, else skip.

| value | name | set by (code) | status |
|---|---|---|---|
| 6 | late-card scene | step 6 `FUN_140446390` arm (A5Judge §3.2); consumed at `0x1405F5225+1` `CMP [RSI+0x11B0],6` (`E.cmp6`; enclosing `FUN_1405F5190`, base reg unresolved) | INF (reader base OPEN) |
| 7 | other-side marker teleport | `FUN_140926E40`: when `MI+0x11B0 == 7` (`E.cmp7`) the side test flips (`!=` vs `==` `MI+0x1558`), teleporting the defending side (`FUN_1409232C0` + `FUN_14092E7E0`) | CONFIRMED mechanism / name HIGH |
| 8 | substitution demo | step 8 `FUN_140446F80` → `FUN_140484A10` (A5Judge §3.2 line 171) | INF (not re-read; W1 arm CONFIRMED) |
| 0xB | out-of-play hold, no placement | step `0x0B` `FUN_140447690` (2.5 s); match-side falls to `926b00` default (`7c9 = 0`) | CONFIRMED (default fallthrough in decompile; W1 arm CONFIRMED) |
| 0xD | other-side marker refresh (own-target copy) | `FUN_140926C90`: `CMP [RSI+0x11B0],0x0D` (`E.cmpD`); on `0xD` takes `param_1+0xACA0`, else the budget path; shared handler with `0x10` | CONFIRMED |
| 0xE | fade-in 1.0 s | step `0x0E` `FUN_1404488E0` | INF (no cmp-imm reader image-wide — W1 + step handler only) |
| 0x10 | restarting-side budget placement | same `FUN_140926C90` handler (non-`0xD` arm → `FUN_140929610` budget move); `0x1410D96C9` is a stack copy (`CMP qword [RBP+0x11B0],0x10`), not an MI reader | CONFIRMED handler arm / name HIGH; CORRECTION: `0x1410D96CC` is RBP-relative, not a direct MI reader |
| 0x12 | UI/sequencing hold | step `0x12` `FUN_140448760/0x1404485F0` | INF (no cmp-imm reader image-wide — W1 + step handler only) |
| 0x13 | formation-plan / officials pass | `FUN_1404405D0`: two direct MI readers `0x1404408FE+1` + `0x140440983+1` (`E.cmp13/13b`, RAX/R15 = MI `local_50`); TRUE arm writes plan flags (`param_1+0x24 = 0x1818`, `0x10E4/0x10E5 = 0x18`) | CONFIRMED |
| 0x14 | teleport 1..20 | step `0x14` `FUN_14044A2A0`: `Flow+0x27D8 = 1` (`D.set1`), `++` per tick (`D.inc`), gated on `CMP [R8+0x11B0],0x14` (`D.cmp14`, R8 = MI); lane gate `0x1405CE57B+1` (`E.cmp14.lane`) | CONFIRMED |
| 0x15 | placement finish | step `0x15` `FUN_14044BB40`: `MOVZX ECX,[RDI+0x11B0]` (`E.movzx15`, RDI = MI), `SUB ECX,0x15` (`E.cmp15rel`) → `FUN_140A3CD90(Flow+0x2790,3)` | CONFIRMED (decompile + bytes) |
| 0x16 | substitution/UI window + think gate | `FUN_14044BB40` else-arm (`SUB ECX,1`, `E.cmp16rel`) → window teardown; `FUN_140926B00` cases `14/15/16` → `FUN_1409270F0` finish; think gated `CMP [R13+0x11B0],0x16` at `0x140929A51` (`E.cmp16`) | CONFIRMED |

CORRECTIONS to `pes-re-team-status.md` §4: (1) `0x1410D96CC` is `CMP qword [RBP+…]`
(stack copy, opcode `48 83 BD`), not a direct `MI+0x11B0` reader — `E.cmp10` pins the bytes
but the base is the frame. (2) `0x140A6BBA0` (`0F 80 …` = `JO rel32`) and `0x140A6BBC8`
(`09 80 …` = `OR [RAX+0x11B0BE],EAX`) are FALSE POSITIVES from the `B0 11 00 00 13`
pattern — the only true `0x13` cmp-imm readers are the two in `FUN_1404405D0`.

## 4. Order in a restart — live sample (read-only, sparse poll; INF)

`live11b0.sh` / `sample11b0.sh` (this scratch; `dd`-based reads, 0.2–0.3 s cadence —
NOT frame-level). Observed on the owner's match (one outfield player/side):

- Stopped (`MI+0x116C = 2`) parks at step `0x16`; the low byte walks `0x16 → 0x00/0x01`
  while `restart = 1` (kick-off pending, `MI+0x1554 = 1`).
- Set-play ready (`MI+0x116C = 3`) holds `0x16` with `11B2/11B3 = 01 01`.
- At the take, `play 3 → 1` (in play) coincides with `restart 1 → 2` on one sampled stop
  (throw-in) — one 0.3 s window, single observation.
- The `0x14` 20-tick window, `0x15 → 3`-record, `0x16` teardown order is code-order
  (CONFIRMED static, §§2–3); the per-frame programme order (`3 → 0xB → 0x45 → 0x11 →
  0x16/0x12/0x13 → 0x14 (×20) → 0x15 → 0x0D/0x0E → 0x34/0x19 → 0x1A`) is A5Judge's builder
  sequence, re-confirmed here only for the `0x14/0x15/0x16` tail. A frame-level sample of
  the byte across 2–3 restarts (which frame advances each value) is still OPEN.

## 5. Port spec — which GF state carries the restart step

Read-only `src/sim` (`origin/main` checkout at `<workdrive>/GodotFootball`):

- Referee buffer: `RefereeBuffer` (`referee.hpp`; fields `desiredSetPiece/teamID/stopTime/
  holdTime/prepareTime/startTime/restartPos/taker/…`, ctor `referee.cpp:127-145`) carries
  the SCHEDULE, not the step. `ScheduleStop` (`referee.cpp:110-125`) derives it from
  `pes::BuildProgramme(kind)` + `pes::RunProgramme(…)` (`pessequencer.hpp:52-58`,
  `pessequencer.cpp:92-133`): beats Hold/Wipe/UiAndTeleport/FadeHalf|FadeIn/Tail with
  `holdKind` 1 (foul, table[1]) / 5 (out-of-play, table[5]).
- Sequencer (`pessequencer.hpp:42-47`): `ReleaseFrames`/`MayRelease` gate the take;
  `ReleasesRestart` (`referee.cpp:228-243`) releases goal kick via the ladder and
  throw-in on taker arrival — the GF analogue of the taker-release gates. No per-tick
  programme cursor exists: GF jumps schedule → armed (`startTime`), PES steps through
  `0x878` states with the `0x11B0` mirror.
- Holder recompute gate (`0x140458912`): GF has no `TA+0x11`, no `MI+0x11B0`, no phase
  switch (`team.cpp:570` ticks score rows outside the in-play gate; `pesreceive.hpp:236-237`
  names W1 `0x14045725A` as the holder writer stand-in; `humanoid.cpp:1327-1400` runs
  `CarrierFromTouch` on a touch, latches a GF holder (`:1343-1345`), and clears it in the
  retain block (`:1387+`, `match.cpp:502`)).
- What the gate needs (add when the writer lands; no GF edit made here):
  1. a `uint8_t restartStep` on the match state (NOT the referee buffer — it is per-tick
     PES state like `MI+0x1168/0x116C`, mirrored per tick, default 0);
  2. a latched `bool holderRunArmed` (set on the first step-`{8,0xB,0xE,0x12}` tick,
     never cleared — `0x140457087/098` semantics);
  3. gate the per-tick recompute on `step ∈ {8,0xB,0xE,0x12}` (arm+run, `DL = 1`) vs
     `{0x14,0x15,0x16}` (run only if latched, `DL = 0`) vs skip (all other steps) —
     replacing the distance-gated `CarrierFromTouch` + the `humanoid.cpp:1343-1345`
     latch and the `:1387` retain-block clear (which PES has no counterpart for);
  4. keep the existing ponytail names (`0x140458912/0x140458960`/`FUN_140457070`,
     `G+0x2A80` probe) until that cutover.

## 6. Status ledger

- CONFIRMED: mirror bytes + RSI/MI wiring (`A.*`); W1 switch + full 15-entry table
  (`B.*`); init/reset stores (`C.*`); Flow `0x27D8` set/inc + `0x14` gate (`D.*`);
  consumers `7/0xD/0x13/0x14/0x15/0x16/6` bytes (`E.*`); `926b00` case set (no
  `6/8/0xB/0xE/0x12/0x13` → default); `926e40` side-flip; `926c90` `0xD` own-target arm;
  `44bb40` `0x15→3` / `0x16→teardown`; `0x14` counter loop; container class identity
  (vtable/slots/factory/ctor/size); no-`E8`-caller + no-XMM-store eliminations;
  checker GREEN + mutation RED.
- INF (high): phase names for `6/8/0xE/0x12` (W1 arm + A5Judge handler map, not re-read
  here); `7/0x10` names (branch CONFIRMED, label by elimination); restart order (§4,
  sparse live poll + code order); stock GF-loader equivalence (untouched).
- OPEN: per-value `O+0x728` writer (register moves found, value flow untraced); O's
  per-tick caller and live instance; `0xE/0x12` consumers beyond W1; frame-level
  step-advance sample; which menu/setup record yields nonzero `A29C`/class 6-7
  (unchanged from team-status).

## 7. Recommended next steps

1. Trace `R13` into `FUN_14041D100` (its caller passes the `+0x728` value) — one `dec.sh`
   region per session rule — to close the per-value writer.
2. Frame-level live sample of `MI+0x11B0` across 2–3 restarts (which frame/event advances
   each value) to close §4.
3. Apply the two CORRECTIONS (§3) to `pes-re-team-status.md` §4, then add
   `uint8_t restartStep` + the `0x140458912` gate (§5) as the wired piece.
