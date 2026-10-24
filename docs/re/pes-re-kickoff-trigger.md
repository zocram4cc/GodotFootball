# PES kick-off trigger: the armed-body scan, live evidence, row 0x15, and the GF port spec

Scope: unblock `pes-port-KickoffRelease.md` (the 125-frame stand-in): decode A5Judge §9.4's scan loop statically, confirm it live from `kickoff_actions.log` + tonight's watches, read true concept row 0x15 live, and spec the GF port. Read-only throughout: Ghidra bridge (one request at a time), `sudo -n` pread on pid 2510857, small reads. Scratch: `<workdrive>/gf-KickoffTriggerRE-scratch/`.

Verdicts: **CONFIRMED** = disassembly/decompile read this session or live pread; **INF** = inference; **OPEN** = blocked, blocker named.

## §0 Constant table (script-read; never typed)

- `0x14256ae34` = 1000.0 (live pread of the VA; the timeout's MULSS at `0x140a437ab`).
- Timeout bitmask `0x11610` → counter arms on `param_1+0x8c` in {4, 9, 10, 12, 16} (decoded from `BT ECX,EAX` at `0x140a43770`).
- Timeout threshold 2000 (`CMP ECX,0x7d0` at `0x140a437bb`).
- `0x1425984c0` = MatchControl vtable (findvt.py); live MC=0x3dc8c5d0, Flow=0x27400010, judge=0x27400918 (vtable `0x1425c50f0` at Flow+0x908 CONFIRMED live).

## §1 The scan loop, statically (CONFIRMED)

Handler `FUN_14044d2e0` (step `0x1a`, armed); release scan `0x14044d62a..0x14044d6ae` (raw disassembly + decompile `d_0x14044d2e0.c` lines 130-153). Operand-VA pairs: `0x14044d63c IMUL RCX,RAX,0x74`; `0x14044d640 MOVZX EDX,[RCX+R14]`, R14=`*(G+0x180)` (`0x14044d611 MOV RCX,[RAX+0x180]`, G from `0x1436f3dc8` at `0x14044d606`); `0x14044d654 CALL FUN_140a66ff0`; `0x14044d675 MOVZX ECX,[RSI]`; `0x14044d680 AND EAX,0x7f`; `0x14044d68b LEA EAX,[RDX-0x12]`; `0x14044d68e CMP AL,0x1`; `0x14044d692 CMP CL,0x3a`; `0x14044d6bf CALL FUN_140a43750`; `0x14044d6e0 CALL FUN_140453530`.

Reconstruction: `T=*(G+0x180)` (0x18 entries, stride 0x74); `R12=*(**(Flow+0x17860)+0x68)` (player array, 0xb4 stride; Flow=driver param via `FUN_140422030`→`FUN_140445e90`→`FUN_14044c880`→`FUN_14044d2e0`, asm pairs `0x14042225a/25e`, `0x140446299/29c`, `0x14044cc87/88a`). Per entry: gate=`T[i*0x74]`; if gate in {0,2} (`CMP DL,1; JC; CMP DL,2; JNZ skip`): `P=R12+i*0xb4` (`FUN_140a66ff0`, CONFIRMED: idx<0x18 → base+idx*0xb4); `Q=P+0xa0` (`FUN_140a44070`, CONFIRMED); if `*(u32*)(Q+0xe4a0)==0` (`FUN_140a60f00`, CONFIRMED): `a=*P&0x7f`; release iff `*P<0xf8`, `a!=0`, `(a-0x12)>1`, `a!=0x3a` (`CMP CL,0xf8/JNC`; `AND EAX,0x7f/JZ`; `LEA-0x12/CMP 1/JBE` kills exactly 0x12,0x13; `CMP CL,0x3a/JZ`). Then `FUN_140a43c90(lVar20,1)` sets the release flag; `FUN_140a43750` timeout backstop (§1.1); `FUN_140453530(Flow)` releases.

Findings vs the ticket: (1) entry byte +0x00 is the GATE, not the action — the action byte is `*P`, first byte of the `R12+i*0xb4` player element in a different array (ticket phrasing corrected). CONFIRMED. (2) Release values: `*P<0xf8` and `(*P&0x7f)` not in {0,0x12,0x13,0x3a}. CONFIRMED. (3) Entry layout: byte 0 = slot/gate state (live §2); no kicker compare in this loop — the `T[i*0x74+0x20]==MI+0x173c` compare is the second loop (`0x14044d7a0..`, `CMP [RCX+R14+0x20],EDX`, `EDX=[MI+0x173c]`, `CMP EDX,0x15` guard) for the 0x1b/0x1e/0x1d kicker-map refresh. CONFIRMED. (4) Which action id the kick-off kick writes, and where: OPEN — `FUN_140782310` returns sub==0x16 (CONFIRMED) but that is the anime entry guard, not the scanned byte; `FUN_1407819F0` shows only the loop clip (row 0xC2 col 9) and in-play MOVE hand-over; the kick's writer of `*P` not found. OPEN (needs the `*P` writer on the kick path or one live `*P` capture at release).

§1.1 Timeout `FUN_140a43750` (CONFIRMED): counts `+0xe5a4` only when `+0x8c` in {4,9,10,12,16}; releases when `counter*1000.0/rate>2000` (rate=`*(FUN_14149a470()+0x74)`; rate identity OPEN). Backstop, not the trigger.

## §2 Live evidence (CONFIRMED pread; INF marked)

Caveat: tonight's match runs polluted geometry (keeper sometimes the taker; kickoff-counter-capture.md). Structure below does not depend on it.

- Kick-off releases (all kicker 10 unless noted): frames 5697→5831 (134), 17039→17173 (134), 29022→29156 (134), 35492→35626 (134), 52647→52779 (132), 67764→67897 (133); ac at release 56 every time. CONFIRMED (kickoff_actions.log TRANS rows + watch.log + trans.log + sub2.log).
- Counter `MI+0x11AC`: 0 until ~70 frames after 2→3, then counts; 56 at every kick-off release. CONFIRMED.
- Entry byte +0x00 is per-tick slot/gate state, NOT the release byte: e00.b0 toggles 00↔54 every 10 frames through the armed window (17049:54 … 17119:00 … 17129:54 … 17159:00 … 17173:54); at release dumps e00.b0=54 in 14/15 sampled releases (first kickoff's release dump reads 00 — sampler raced the transition). The brief's "3 → 1" byte is INF to be this same toggling gate byte (its 3→1 phrasing matches a gate-state change at the transition, but no 10-frame sampler can resolve the release tick; the per-tick value at the release dump is 0x54 in nearly all samples, not 1). What the log CONFIRMS: every other entry/offset is static across the window (only e00.b0 and the +0x28/+0x50 pointer slots move), so the release is written to the player array `*P`, not to `G+0x180` — consistent with §1.
- Timing source (CONFIRMED): the taker's approach. Kicker standby (`pes-re-kicker-standby.md` §2, CONFIRMED): only kick-off loops (22/23) return false from slot 10, so default locomotion steers the taker; every other standby holds. `FUN_1407819F0` plays `kickoff_loop_kicker` (row 0xC2) while locomotion walks him; `FUN_140782310` hands to the kick at sub==0x16. No clip-contact frame or timer was found on this path (SetKick is the no-op `0x1406DEB60` for kick-off): the 132-134-frame window is approach time (INF: varies with geometry — keeper-taker kicks read 139, counter-capture.md). No second opponent-dependent gate sampled (A5Judge §9.2/OPEN-9 stands).
- Scene branch not taken: `[G+0x400]` first byte 0 at every transition (2→3 and 3→1), so `FUN_141F9C3B0` is false and the scan path runs. CONFIRMED.
- NOT confirmed live: `R12`/` *P` bytes (Flow+0x17860 reads null outside the armed tick and my transition-triggered reads raced it; judge Flow object itself CONFIRMED via vtable). The live `*P`-at-release capture is OPEN (needs a per-tick sampler keyed on the armed Flow, or the static `*P` writer).

## §3 Concept row 0x15 (CONFIRMED live, open play)

`H=*(0x1436F3F98)=0x3dc7d220`; `C0=[H+0x450]=0x3dc7dbe0`, `C1=[H+0x458]=0x3dc849e0`; `row=C+0x62B8+0x488`. Both sides identical:

- vtable `0x1425C4200`; methods `[row+0]=0x1404F0DB0`, `+0x8=0x140558A10` (Attacking Area == 1/Centre gate, per concept-index.md §0), `+0x10=0x1405625F0`, `+0x18=0x1405624E0`, `+0x20=0x140562490`, `+0x28=0x140563750`, `+0x30=0x141863350` (stub).
- Identity: the `0x140558A10`-gated (Centre) row at true id 0x15 with the `562xxx` family — the Centre-attacking row the old dump skipped (its `idx0x14 off+0x488 NULL-ARM` line conflated NULL slot 0x14 with this row). Row name/behaviour: OPEN (bodies `5625F0/5624E0/562490/563750` not decompiled; one bridge session).
- Live payload (open play, play=1): `T` gate bytes `e00=00`, `e01=02/01`, `e02..e23=01/idx` — the pad/slot table at rest.

## §4 Port spec for GF (replaces the stand-in)

- Current stand-in: `pessequencer.cpp:30` `kKickOffReleaseFrames = 125` (per brief; on this checkout the file holds corner/FK/PK frame gates with the kick-off/goal-kick `ReleaseFrames` default 0 = receiver-test/ladder — either way a fixed stand-in, not the scan).
- Port (literal, when the `*P` writer / ANIME action ids land): per tick while armed, for each of the 0x18 pad entries with gate byte in {0,2}, read the mapped player's action byte `a` (masked `&0x7f`, range-checked `<0xf8`); release when `a` not in {0,0x12,0x13,0x3a} (exact conditions §1), else the `FUN_140a43750` timeout backstop. Delete the fixed frame count in the same change (clean cutover; no shim). Until then: keep the shortest-measured-window gate as the named `// ponytail:` stand-in (132-134 live tonight; 118 with polluted geometry; the 125 stands).
- Blocked on: OPEN item §1(4) — the kick's `*P` value (static writer or live capture). Next step: decompile the `562xxx` row only if a Centre-attack port is wanted (not needed for kick-off); for kick-off, hunt the `*P` writer on the kick path (`FUN_140A6CA10`-family / `+0xC8` SetAction chain) or run one armed-tick `*P` sampler.

## §5 Status ledger

| item | verdict | evidence |
|---|---|---|
| scan tests gate `T[i*0x74+0x00]` in {0,2}, action `*P=R12[i*0xb4]+0` | CONFIRMED | `0x14044d63c/640/654/675/680/68b/68e/692` asm + `FUN_140a66ff0/44070/60f00` decompiles |
| release values `*P<0xf8`, `&0x7f` ∉ {0,0x12,0x13,0x3a} | CONFIRMED | `0x14044d678/680/683/68b/68e/690/692/695` |
| `FUN_140a66ff0` stride 0xb4, `FUN_140a44070` +0xa0, `FUN_140a60f00` +0xe4a0==0 | CONFIRMED | decompiles this session |
| timeout backstop shape | CONFIRMED | `FUN_140a43750` decompile + asm; rate identity OPEN |
| Flow chain to driver param | CONFIRMED | `0x14042225a/25e`, `0x140446299/29c`, `0x14044cc87/88a` |
| kick-off release = scan, not receiver gate/timeout | CONFIRMED | ac=56 vs gate's >972 need (counter-capture) + scene byte 0 tonight |
| arm→release 132-134 f, ac 56; counter starts ~70 f after arming | CONFIRMED | 6 kick-offs tonight + log TRANS rows |
| timing = taker approach (locomotion loop + sub-0x16 hand-over) | CONFIRMED structure / INF exact contact | standby doc + `7819F0/82310` decompiles; no contact frame found |
| kick's `*P` value / writer | OPEN | needs writer hunt or armed-tick `*P` capture |
| row 0x15 vtable+methods | CONFIRMED | live pread both sides, 0 diffs |
| row 0x15 name/behaviour | OPEN | bodies not decompiled |
| port spec | INF (blocked on `*P` writer) | §4 |
