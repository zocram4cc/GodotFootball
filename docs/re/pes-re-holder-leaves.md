# PES ball holder leaves: TA+0x11, T+0x78, 0x1404582C0, 0x140802DC9, P+0x19BA, W1 cadence

> **Main audit 23-10-26 (bytes):** `0x140457087 C6 41 11 01 mov byte [rcx+0x11], 1` (W1 sets TA+0x11) and `0x1407F4876 mov byte [rsi+0x2472], 1` (P+0xAB8+0x19BA = P+0x2472): match.

Read-only RE. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request at a time. Base doc: `docs/re/pes-re-holder.md` (HolderRE; W1 `0x140457259`, W2 `0x140A71DF9`, `FUN_140A63020` 3-level rule, step-43 gate).
Checker + evidence: `<workdrive>/gf-HolderLeavesRE-scratch/check_holder_leaves.py` (CLEAN, 20 rows incl. 2 mutation rows), decompiles `d_*.c`, xrefs `xref_*.txt` in the same dir.
GF paths below are `origin/main` (`<workdrive>/GodotFootball`).

## §0 Constants (checker output, pasted: script-read, never typed)

| VA | value |
|---|---|
| `0x14259A764` | 60.0 (neighbor `0x14259A760` differs — mutation row) |
| `0x142588218` | -1.0 (`0x1407F47C0 MOVSS XMM6,[…]`) |
| `0x14256AE1C` | 1.0 (`0x1407F47D1 MOVSS XMM1,[…]`, `0x1407F4839 MOVSS XMM6,[…]`) |
| `0x14258C264` | 3.0 (`0x1407F47D9 MOVSS XMM0,[…]`: `_DAT_14258C264 − *pfVar3`) |
| `0x14259858C` | 360.0 (`0x1407F4841 MULSS XMM6,[…]`) |
| `0x142598580` | 180.0 (`0x1407F482B COMISS XMM0,[…]`) |
| `0x142598578` | 0.2 (`FUN_140781780` lockout `uVar9`, `FUN_1407F4760` line 75) |

## §1 The caller that sets TA+0x11 (W1 runs only when set) — CONFIRMED mechanism, INF phase names

Rule (decompile + asm + jumptable, all checker-pinned):

- W1 (`FUN_140457070`) sets `TA+0x11` itself: `0x140457087 C6 41 11 01` = `MOV byte [RCX+0x11],1` when `param_2 != 0` (decompile lines 40-44), then gates the whole body on `0x140457098 80 7D 11 00` = `CMP byte [RBP+0x11],0; JZ return`. So `TA+0x11` is a **latch**: no clear site exists (byte-pattern sweep `C6 41 11 00` finds 5 hits, all in `0x1401Axxxx`/`0x141A0xxxx`/`0x1422Dxxxx`/`0x14B40xxxx` R9-based stores, none in TA code). Once any `param_2 != 0` call sets it, every later call runs the body even with `param_2 == 0`.
- The only caller of W1 is `FUN_140458880` (`0x140458940 CALL`, single xref; §3). It decides `param_2` from `MI+0x11B0` (`0x140458912 41 0F B6 87 B0 11 00 00` = `MOVZX EAX,byte [R15+0x11B0]`, `R15` = MI): `ADD EAX,-8; CMP EAX,0xE; JA default` (bytes `83 C0 F8 83 F8 0E 77`, checker row), jumptable at `0x140458960` (15 RVAs, checker row): phases `8/B/E/12 → DL=1` (`0x140458937 B2 01`), phases `14/15/16 → DL=0` (`0x14045893B 33 D2`), all other phases → skip W1 entirely (default `caseD_9` returns past the call).
- So: W1 **runs with set** in phases 8/B/E/12; **runs without set** (body only if latched) in open-play phases 14/15/16 (net-dodge: "`MI+0x11b0` (open play 0x14-0x16)"); **is not called** in any other phase. First set happens at the first 8/B/E/12 tick; from then on open-play ticks recompute the carrier every call.
- Phase-name mapping for 8/B/E/12: OPEN (names live in foundation/set-piece docs: A5Judge mirrors `MI+0x11b0` as the teleport counter, 0x15→0x16 finishes placement; `MI+0x11b0` = "AI tick mode" per foundation E7 row, writer unknown). What blocks closing: the per-value meaning of 8/B/E/12.

GF carriage: no 1:1. GF has no TA struct, no `MI+0x11b0`, no latched run flag. The W1 recompute itself is `CarrierFromTouch` (`pesreceive.hpp:244`, named stand-in for the newest ring-1 K=8 key; `G+0x2A80` probe OPEN). What GF needs when the phase map lands: gate the per-tick recompute on (phase in {8,B,E,12,14,15,16}) with the latch (run-if-ever-set), not on ball distance. Until then the GF latch (`humanoid.cpp:1343-1345`) + retain-block clear (`:1387`, which PES has no counterpart for) stay as named stand-ins.

## §2 The T+0x78 writer — OPEN (all reads, no store found)

Rule (CONFIRMED reads, OPEN writer):

- `FUN_140A48A30(G,side)`: `side<2 → *(G+((side+0xA1)*0x28))` else `*(G+0x1928)` (10-line body, decompiled). The team struct T it returns carries `+0x78`, read as a player id (`< 0x16` validity at every site).
- Read sites CONFIRMED: `FUN_140A63020` 3rd level (`uVar3 = *(lVar4+0x78); if < 0x16 return`), `FUN_140461C10` lines 105-107 (`param_1[0x4C]+0x78`, `param_1[0x4D]+0x78` per side, copied to `param_3[0..1]`, then holder-setback `FUN_140A72A60` if holder disagrees), `FUN_1404F8AD0` (`*(lVar6+0x78)==uVar15` steal-gate), `FUN_140A23C10` lines 416-419/712-717/759 (own-side vs other-side select on `T+0x255`, fallback `FUN_140A62960`).
- Writer: OPEN. Swept: holder setter `FUN_140A72A60` (only writes `+0x2483C`), all cached team-region decompiles (`140A62180/140A62780/140A62960/140A62B30/140A638C0/140A639A0/1404566A0/140461420/140461C10/1404F8AD0/140A23C10`) contain zero `+0x78` stores; image-wide disp8-78 store patterns too noisy (2507/1185) without a per-function map. Next: decompile the remaining `FUN_140A48A30` callers that take T (40+ sites in `xref_A48A30.txt`) and grep each for a `+0x78` store, or find who writes the `param_1+0x4A`-frame slots.

GF carriage: no 1:1. GF stand-in is `kickMan/kickValid` (`teamAIcontroller.cpp:2440-2442`: "this side's last kick, valid when the newest touch is this side's"; consumed at `:2496`, stored at `:2528/:2566`). The ponytail there already names the missing piece ("the opponent-kick fallback T_opp+0x78 (OPEN writer) is absent"). Nothing to change until the writer lands.

## §3 Callers: 0x1404582C0 and the function enclosing 0x140802DC9 — CONFIRMED

- `FUN_1404582C0` has exactly one code caller: `FUN_140426760` (`0x14042678E E8 2D1B0300 → 0x1404582C0`, sole entry in `xref_4582C0.txt`). `FUN_140426760` body (8 lines, decompiled): `if (*(param_1+0x10)) FUN_1404582c0(*(param_1+0x10),param_2)` — the listener dispatch for message 2. Full static chain: `FUN_14042B9F0` (msg-2 arm: `!bVar4 && !bVar5` and `vt+8` true → `FUN_140426760`) → `FUN_1404582C0` → `FUN_140458880` (sole call `0x14045832B`) → `FUN_140457070`/W1 (sole call `0x140458940`). `FUN_140426760`'s own caller list is empty on the bridge (vtable/fptr dispatch — consistent with the listener object at `param_1+0x40`); `FUN_14042B9F0` likewise has no code xrefs.
- `0x140802DC9` is inside `FUN_140802B60` — CONFIRMED (bridge `disassemble_function?address=0x140802DC9` returns `140802B60`-based lines; bytes `3B 81 1C 60 02 00 0F 94 C0` = `CMP EAX,[RCX+0x2601C]; SETE AL`, tail `83 7B 14 00 88 44 24 70` = `CMP [RBX+0x14],0; MOV [RSP+0x70],AL`, both checker rows). Decompile line 265: `bVar38 = iVar10 == *(int *)(local_2b0 + 0x2601C)` where `local_2b0 = *(vt-call(plVar36)+0x18)` (MA via the `+0xAB8` action object `plVar36 = param_2+0xAB8`), `iVar10 = (vt+8)(plVar36)` (the man's id). So: carrier-equals-me test in the per-man anime/move function (`FUN_1407FA2C0` called at lines 256/387, `FUN_140806F00` chase at 807/840/856 follow). GF: the same compare exists wherever GF tests `GetBallRetainer() == player` (e.g. `humanoid.cpp:1347/:1485`), no new state.

## §4 Writer of P+0x19BA (mirror-cycle flag) — CONFIRMED via sub-object alias P+0x2472

Rule (bytes + decompiles, checker-pinned):

- The reader `0x1406EC700: 0F B6 81 BA 19 00 00` = `MOVZX EAX,byte [RCX+0x19BA]; RET` is `vt+0x30` of the vtable at `0x1425E3508` (qword at `0x1425E3538` = `0x1406EC700`, checker row). Installers `FUN_1406E03D0`/`FUN_1406E1610` write `param_1[0x157] = vtable` → the sub-object lives at `P+0x157*8 = P+0xAB8` (both decompiled; `LEA R14/R15,[RDX+0xAB8]` at `0x140781790`/`0x1407F476F` confirms the call pattern `RCX = P+0xAB8`).
- Hence `RCX+0x19BA` (sub-object-relative) = `P+(0xAB8+0x19BA)` = **P+0x2472** (player-relative). The "zero direct stores image-wide" elimination in the base doc is explained: writers use the player-relative displacement. Capstone sweep of disp32 `0x2472`: 87 unique sites, all `MOV byte ptr [reg+0x2472], X`, classified: 1× `=1`, 6× `=0`, 80× register.
- The `=1` writer: `0x1407F4876 C6 86 72 24 00 00 01` in `FUN_1407F4760` (decompiled, 40 lines). Rule: `fVar4 = wrap180(angle − P+0x20)` where `angle = FUN_140439E50(...) − *kickdir` scaled by `−1.0 or 1.0 (FUN_140A6A1C0 side test) × pitch 5 of kick record`; if `|fVar4| > 180.0` (`COMISS XMM0,[0x142598580]`): subtract `±360.0` (`MULSS XMM6,[0x14259858C]`); then `fVar4 <= 0.0 → P+0x2472=1 + set P+0xAD0 bit 0x1000000000` (`OR`, at `0x1407F487D`), else `=0` + clear that bit. I.e. the flag is the **sign of the wrapped angle delta** (mirror selector), twinned with a bit in `P+0xAD0`.
- The `=0` writers (6): `0x14078182D` (`FUN_140781780`, else-arm with `FUN_1406F0F70` + `P+0x2471/0x2478 = 0`, `P+0x245C = 1`), `0x1407F485C` (same function, other wrap arm), `0x1407A8874`, `0x1407C66AC`, `0x1407F485C`/`0x1407F485C`-class, `0x1407C2922`-adjacent `sil` etc. are register-writes; full list in scratch (`seen` dump). The 80 register writers (`al/bl/sil/dil/r9b/r12b-r15b…`) are per-clip/per-phase flag sets across `0x1406Bxxxx-0x14080xxx` (move-request builders incl. `FUN_1407457E0`, the `+0x2601C` reader site). Any one of them can set or clear the flag; the documented default at reset/init is 0 (memset-class init + the six `=0` sites).
- Reader/writer pairing CONFIRMED structurally: `FUN_1407F4760` head calls `(P+0xAB8)[vt+0]` twice (the same sub-object the reader serves), then writes `P+0x2472`.

GF carriage: 1:1 state exists but is stubbed: `ClipFlag()` (`pesmovestep.cpp:396`) returns `false` with ponytail "`P+0x2472`, the anime interface's flag byte (vt+0x30); its writer is not decoded yet." Port: back `ClipFlag()` with the wrapped-angle rule above (inputs: GF has the man's yaw `P+0x20`-equivalent, the kick record, `FUN_140A6A1C0` side; needs `FUN_140439E50` angle — name it) and set/clear the `P+0xAD0`-bit twin wherever GF carries `0xAD0` (check: GF likely has no `0xAD0` word yet — if so the flag alone is the cutover and the bit is a named OPEN). The 4-cycle mirror map stays as decoded. Deliver to ClipRoot: replace the ponytail at `pesmovestep.cpp:395-396` with this rule.

## §5 W1 cadence vs the team tick — INF (chain CONFIRMED, order OPEN)

Static chain CONFIRMED: listener `FUN_14042B9F0` msg-2 arm → `FUN_140426760` → `FUN_1404582C0` → `FUN_140458880` (phase switch) → `FUN_140457070`/W1. Team tick `FUN_1404F6C30` is called by a direct `CALL E8 54 7E 0D 00` at `0x14041EDD7` (target arithmetic verified `0x14041EDDC+0xD7E54 = 0x1404F6C30`, checker row), but the enclosing function is unanalyzed on the bridge (no function at `0x14041EDD7`/`0x14041ED98`; `RET` at `0x14041ED97`, small ctor `FUN_14041EC80` adjacent — Denuvo/VM region). So the frame order (listener-msg-2 before/after team tick) is OPEN. What is known: W1 writes the carrier in the listener path; the team tick only reads it (`FUN_1404F8C70` via `FUN_140A63020`, `FUN_1404F8AD0` via `FUN_140A627E0`); step-43 sweep takes no carrier input. For HolderSplit: recompute-then-latch per tick preserves PES order under either interleaving (write-then-read), so the latch timing (§6.3 of base doc) is safe to port as stated.

## §6 GF state table (read-only search, `origin/main`)

| PES piece | GF 1:1 | GF file:line |
|---|---|---|
| MA+0x2601C slot | `match->GetBallRetainer()/SetBallRetainer` (`ballRetainer`) | `match.hpp` via `humanoid.cpp:1227/:1333/:1345/:1933` set, `:1387` + `match.cpp:502` clear |
| W1 recompute (newest ring-1 key or 0xFF) | stand-in `CarrierFromTouch` + `GetLastTouchPlayer()` (ponytail names `G+0x2A80`) | `pesreceive.hpp:244`, `teamAIcontroller.cpp:2427` |
| 3-level fallback carrier→holder→T+0x78 | partial: `GetBallRetainer()` else `GetDesignatedPossessionPlayer()`; T+0x78 leg = `kickMan/kickValid` stand-in | `teamAIcontroller.cpp:408/551/2622, 2440-2442/2496/2528/2566` |
| holder `MA+0x2483C` setter | `FUN_140A72A60` has no GF counterpart yet (OPEN input, named in base §6) | — |
| T+0x78 writer | none (OPEN); stand-in documented | `teamAIcontroller.cpp:2429` ponytail |
| TA+0x11 / MI+0x11B0 / phase switch | none (OPEN) | — |
| P+0x2472 mirror flag | stub `ClipFlag() == false` (ponytail) | `pesmovestep.cpp:395-396` |
| DC9 carrier-equals-me | `GetBallRetainer() == player` tests | `humanoid.cpp:1347/:1485` e.g. |

## §7 Status

| item | status | evidence |
|---|---|---|
| TA+0x11 setter/cadence rule | CONFIRMED mechanism / INF phase names | `0x140457087/098`, `0x140458912-93B`, jumptable `0x140458960`, checker |
| TA+0x11 clear | CONFIRMED absent (latched) | 5-hit sweep, all off-target regions |
| T+0x78 writer | OPEN | 10+ decompiles, zero stores; reads CONFIRMED |
| `FUN_1404582C0` caller | CONFIRMED sole `FUN_140426760` | `xref_4582C0.txt`, CALL bytes |
| `0x140802DC9` enclosing | CONFIRMED `FUN_140802B60` | bridge containment + line 265 + checker |
| P+0x19BA writer | CONFIRMED (P+0x2472 alias; `=1` at `0x1407F4876` in `FUN_1407F4760`) | reader/vtable/installer/`+0x2472` sweep, checker |
| W1 vs team-tick order | INF (chain yes, interleaving no) | CALL target arithmetic; VM-region parent |
| float constants §0 | CONFIRMED script-read | checker |

Open for the port: phase-name map of `MI+0x11b0` ∈ {8,B,E,12}; T+0x78 writer sweep over remaining `FUN_140A48A30` callers; GF `P+0xAD0` word (twin bit `0x1000000000`); `FUN_140439E50` angle for the `ClipFlag` backer.
