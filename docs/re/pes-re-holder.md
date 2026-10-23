# PES ball holder MA+0x2601C, decoded 1:1 (HolderRE)

> **Main audit 23-10-26 (bytes):** `0x140457259 mov [r13+0x2601C], edi` (W1), `0x140A71DF9 mov [rbx+0x2601C], 0xFF` (W2), the sweep gate `0x1404F78DB cmp [rax+0x218], 0`: match.

Read-only RE. No repo edits. Image `~/ghidra-projects/scratch/Unpacker/pes_full.bin` (file offset = VA − 0x140000000). Bridge `http://127.0.0.1:8080`, one request at a time. Checker + evidence: `<workdrive>/gf-HolderRE-scratch/` (CLEAN, 46 rows; 3/3 mutations fire).
GF paths below are `origin/main` (`<workdrive>/GodotFootball`); the HolderLatch code cited is the landed latch (`humanoid.cpp:1330-1400`, FarChasePressMain note 22-10-26).

## §0 Constants (checker output, pasted)

| VA | value |
|---|---|
| `0x14256AE1C` | 1.0 |
| `0x14259A764` | 60.0 |
| `0x14256AE34` | 1000.0 |
| `0x142598590` | 3600.0 |
| `0x14259BE20` | 25.0 |
| `0x14259BE30` | 45.0 |
| `0x14259BE44` | 64.0 |
| `0x14259D60C` | 9.0 |
| `0x1425C6780` | 81.0 |
| `0x14259BDF4` | 8.0 |
| `0x14253EE30` | 100.0 |
| `0x142598578` | 0.2 |
| `0x14256AE18` | 0.01 |
| `0x14259BE60` | 135.0 |
| `0x14259BE68` | 150.0 |
| `0x14256AE30` | 120.0 |
| `0x14259BE5C` | 90.0 |
| `0x14257A190` | 0.6 |
| `0x14259BE04` | 12.0 |
| `0x14259BE1C` | 20.0 |
| `0x14259BDD8` | 1.3 |
| `0x14259BE40` | 57.2957763671875 |
| `0x1425795E4` | 6.2831854820251465 |
| `0x14259BDD0` | 0.6944444179534912 |
| `0x14259D65C` | -3.6 |
| `0x142565600` | 0.5 |
| `0x142588218` | -1.0 |
| `0x14351AE50` (u32) | 54 |

## §1 Writers of MA+0x2601C (CONFIRMED: flat disp32 scan, 21 operands, capstone-decoded)

Only 2 writers. Both store a dword player id (0..21) or 0xFF (none, read as u32 `> 0x15` = none).

| # | VA | instruction bytes | function | value written | condition | when in frame |
|---|---|---|---|---|---|---|
| W1 | `0x140457259` | `41 89 BD 1C 60 02 00` = `MOV dword ptr [R13+0x2601C],EDI` | `FUN_140457070` (team-AI tick; MA = `*(param_1+0x508)`) | EDI = actor of newest ring-1 key via `FUN_140A3E7D0(E,·,0)` + `FUN_140A60530` + `FUN_140A62E40`, else `0xFF` (decompile lines 129-150; asm `0x140457204-0x140457259`) | `param_2 != 0` sets `TA+0x11` (decompile line 41-42); the store runs only when `TA+0x11` is set (line 46: `if (*(char *)(param_1+0x11)==0) return`) | team-AI tick `FUN_140457070`, called from `FUN_140458880` ← `FUN_1404582C0` (E8 chain; `FUN_1404582C0`'s own caller OPEN). Sits in the listener step-5 path (`FUN_140426760 ← FUN_1404582C0 ← FUN_140458880 ← FUN_140457070`), NOT the team tick |
| W2 | `0x140A71DF9` | `C7 83 1C 60 02 00 FF 00 00 00` = `MOV dword ptr [RBX+0x2601C],0xFF` | `FUN_140A71C30` (MA constructor/reset: `memset 0xEAE20` at base, then sub-object resets; the `+0x2600C/+0x26010/+0x2601C` stores at `0x140A71C90/0x140A71CA9/0x140A71DF9`) | `0xFF` (none) | unconditional inside the reset | object init/reset, NOT per tick. Caller OPEN |

Split forms: no `lea [0x19BA]`-style split exists for this field either — the full-field search is the flat scan above (21/21 classified: 2 writes, 19 reads). Neighbouring-displacement writers (`+0x26010` at `0x1404573A5`, `+0x26014` reset at `0x140A71C92`, `+0x26018` at `0x140A71CAB`) are different fields (see §5), not split writes of `+0x2601C`.

Reader of record: `FUN_140A627E0` (`0x140A627E0-0x140A6281B`): `param_2==0` returns `*(u32*)(*(param_1+0x18)+0x2601C)` (asm `0x140A627EC: 8B 80 1C 60 02 00`); else the n-th ring-1 key's player or `0xFF`.

## §2 Clears (CONFIRMED)

- The ONLY store of 'none' is W2 (`0x140A71DF9`), at object reset. There is NO per-tick, per-event, per-stoppage or per-set-piece clear of MA+0x2601C in the image.
- W1 re-derives the value from the newest ring-1 key on every team-AI tick call (TA+0x11 set). It never reads a loose-ball flag and never clears on loss: on a loose ball the field keeps the last toucher's id until (a) a newer K=8 key is pushed (`FUN_140A0DFD0`, probe `G+0x2A80` gated, OPEN writer), (b) the newest key maps to no on-pitch slot (→ `0xFF`), or (c) W1 is not called (`TA+0x11` clear; flag's caller OPEN).
- Consequence for the brief: PES has no "clear on loose / deflection / stoppage / set piece". GF's retain-block clear (`humanoid.cpp:1387: SetBallRetainer(0)` when the ball leaves the man) is a GF predicate with no PES counterpart — PES persists by construction (no writer clears it), GF clears by test. The latch (`humanoid.cpp:1343-1345`) re-sets it while live, which is why the two-tick test passes and the one-tick flicker remains.

## §3 Readers that matter for the team tick (CONFIRMED: decompile + asm context)

Validity test everywhere: `id < 0x16` (22, `CMP …,0x15` / `JA`), i.e. 0..21 = player, ≥ 22 = none. The `0xFF` sentinel reads as u32 `0xFFFFFFFF > 0x15`.

| VA (insn start) | function (enclosing) | rule (CONFIRMED decompile/asm) | team-tick relevance |
|---|---|---|---|
| `0x14042DE27` | `FUN_14042DCB0` (match state machine, `param_2+4` case `0x10`) | reads carrier `uVar9`; in-play (`MI+0x116C==1`): `uVar5==uVar9` (newest-key actor is the carrier) → true; else if carrier/holder (`+0x2601C`, else `+0x2483C`) `< 0x16` and matches → true; else newest ring-0 team compare | possession/state gate: carrier equality is the first in-play test; holder `+0x2483C` is the fallback; latch-adjacent (status CONFIRMED structure) |
| `0x140701980` | `FUN_1407017E0` | `carrier > 0x15 && MI+0x1775 != 0x0B`: then candidate admitted if he is the holder (`+0x2483C`), the ranked head (`+0x24840`), or the tracked holder (`+0x24838`) within 1.0 m (`COMISS XMM6,[0x14256AE1C]` = 1.0) | team shape/support candidate gate: no-carrier arm only; holder trio is the fallback (status CONFIRMED structure) |
| `0x140703899` | `FUN_140703860` | `carrier > 0x15` (no carrier): if newest event is this frame (`MI+0x1198 == rec+0x10`), set `param_1+0x1FC = gs×(2 or 0.2)` (`0x142588200` = 10.0, `0x142598578` = 0.2) | reaction-delay timer write on a touch frame with no carrier (CONFIRMED) |
| `0x1407460AE` | `FUN_1407457E0` (move-request builder) | `carrier = [RDX+0x2601C]; if > 0x15 skip; else FUN_140A48910(G,carrier)`, anime-class byte test (`+1 == 0x12`), then NearestList row `MA+0x22E4C+carrier·0xC8` rank byte `+0xB2` (asm `0x1407460AE-0x14074611D`) | movement target row select: carrier picks the list row (CONFIRMED asm) |
| `0x1407F7DB6` | `FUN_1407F7370` (reaction, 67 KB) | `iVar18 == carrier` sets `bVar34` (carrier-is-me test, decompile line 462) | reaction self test (CONFIRMED line) |
| `0x1407FA360` / `0x1407FA9BE` | `FUN_1407FA2C0` (move execution, two call sites, same function) | `iVar6 = vt+0x08()` (the man's id); `bVar18 = iVar6 != carrier` (line 48); second site (line 331): mismatch → bearing `FUN_140439F50` + angle `FUN_140481B00`, refuse if `> 25.0` (`0x14259BE20`) | mover-vs-carrier gate + 25° turn gate (CONFIRMED) |
| `0x1407FB5CE` | `FUN_1407FB470` | `iVar4 = carrier`; `iVar3==iVar4` (I am the carrier) → ball-path branch; else facing `MA+0x247D4` wrap vs 90.0 (`0x14259BE5C`) (decompile lines 79-100) | carrier ball-path vs facing branch (CONFIRMED structure) |
| `0x140802DC9` | (enclosing fn OPEN: bridge decompile endpoint returned empty for this VA; asm `CMP EAX,[RCX+0x2601C]; SETE AL; CMP [RBX+0x14],0; JE; CMP …,SIL; JNE skip`) | `al = (eax == carrier)`; stored to `[RSP+0x70]`; two-match gate (`[RBX+0x14]==0` and `SIL` match) decides the skip at `0x140802DDA/0x140802DE5` | equality-flag consumer (INF: enclosing function not identified; status OPEN) |
| `0x140807631` | `FUN_140806F00` | `iVar11 != carrier` → threshold `150.0` (`0x14259BE68`), else `135.0` (`0x14259BE60`); `FUN_14081E460` distance test (decompile lines 340-348) | chase-distance band select: 135 m carrier / 150 m other (CONFIRMED lines; unit as-read) |
| `0x140930F21` | `FUN_1409307C0` (per-player movement builder; receive §1.2) | anime-class gates, then `carrier > 0x15` → `*piVar1 = 2; reaction+0x878 = (==4)+2` (decompile lines 314-318) | receive entry-3 TRAP gate: no carrier forces reaction state 2 (CONFIRMED) |
| `0x1409DD968` | `FUN_1409DD470` (receive helper) | `carrier > 0x15` AND ball-sample race won → `FUN_14210AA30(…,4,…)` touch record + `gs/12+fVar26` lockout compare (decompile lines 195-215); later (line 350): `carrier<0x16 \|\| holder<0x16` → `FUN_1409D9B40` contest | touch-record + contest (CONFIRMED structure) |
| `0x1409DEC0F` | `FUN_1409DE8C0` (receive helper) | same shape: `carrier > 0x15` AND sample race → touch record (line 187); holder fallback `+0x2483C` at line 339, then `FUN_1409D9B40` | touch-record + holder fallback (CONFIRMED structure) |
| `0x1409DFE2B` | `FUN_1409DF7D0` (receive helper) | `carrier = [R14+0x2601C]; if > 0x15 carrier = [R14+0x2483C]` (asm `0x1409DFE2B-0x1409DFE43`); match newest key team (lines 268-290) | carrier-then-holder read for the newest-key team match (CONFIRMED) |
| `0x140A627EC` | `FUN_140A627E0` | the accessor itself (`MOV EAX,[RAX+0x2601C]`) | the canonical read; all latch/reaction/support reads route here or inline it |
| `0x140A6302A` | `FUN_140A63020(ctx,side)` | carrier `< 0x16` and on `side` → return it; else holder `+0x2483C` `< 0x16` and on side → return it; else `T+0x78` fallback (`FUN_140A48A30` + `+0x78 < 0x16`) | THE team-tick reader: carrier → holder → `T+0x78` side match (CONFIRMED full body, 30 lines). Port spec §6 |
| `0x1420C57EF` | `FUN_1420C5330` (Denuvo-VM region) | `*(param_1+0x45BC) = carrier` (copy out) | carrier snapshot copy (CONFIRMED line) |
| `0x142106596` | `FUN_142106110` (Denuvo-VM region) | `uVar2 = carrier; local_a0 = (uVar2 < 0x16 && local_c8==uVar2)`; `CMP R8D,0x15; JA; CMP EDX,R8D` (asm `0x142106596-0x1421065A6`) | carrier-equals-expected test; **this is inside the step-43 release sweep's caller chain** (run 6-8 context, §4). Status CONFIRMED lines, enclosing sweep mapping INF |
| `0x142161817` | `FUN_142161520` (Denuvo-VM region) | `uVar20 = carrier (via FUN_14087B3D0()+0x2601C)`; `uVar20 < 0x16 → bVar4=true`; gates `bVar5` (carrier-vs-expected with on-pitch `MI+0x1748` test) | carrier-valid + on-pitch gate (CONFIRMED structure) |

Battery note: `0x1407FA360` and `0x1407FA9BE` are the SAME instruction (`CMP EAX,[..+0x2601C]`, bytes `3B 83/81 1C 60 02 00`) at two call sites of one function; the 21-operand scan counts both (press-coord §4a agrees: "21 operands, 2 writes").

## §4 Step-43 release sweep gate (`docs/ports/pes-port-TeamT2.md` runs 6-8)

- Step 43 (`0x1404F78E4-0x1404F7903`, `pes-re-tick-steps3.md:84`): inline loop 11× `FUN_140A40820(T,s)` gated on `T+0x218==0` (asm `0x1404F78DB CMP [T+0x218],0; JNZ skip`, re-verified raw). `FUN_140A40820` (this session, 30-line body): `T+0xA2A0[s]=0`, history-ring push `FUN_140A40200`, zero `T+0x26C/0x28C+0x40s` blocks — releases run states incl. PRESS `0x34`.
- The carrier does NOT gate the sweep: no `+0x2601C` read sits in `0x1404F78E4-0x1404F7903` (the sweep's disp32 reads are `+0x218` only). The `0x142106596` carrier test above is in the sweep's caller chain (Denuvo region), not in the gate: it decides carrier-equals-expected for a run entry, it does not hold or release the sweep.
- So the sweep answers the brief's run 6-8 question directly: the `T+0x218==0` gate fires with the carrier at whatever W1 last wrote — including a stale last-toucher on a loose ball. No carrier read can block it.

## §5 Neighbour fields (not this field; listed so the port keeps them apart)

| field | writer (CONFIRMED) | meaning |
|---|---|---|
| `MA+0x2600C` | `0x1404571F1 MOV [R13+0x2600C],ESI` (same function `FUN_140457070`) | designated owner from the 9-entry ring-0 scan (`MI+0x1168==9` arm; first-touch §5.2) |
| `MA+0x26010` | `0x1404573A5 MOV [R13+0x26010],EDI` (same function) | post-step-1 fallback id: the 3-entry `MA+0x24840` scan winner, else the carrier (asm `0x14045729C-0x1404573A5`; `MOVSS XMM6,[0x14256AE1C]` = 1.0 m² ball-proximity test at `0x140457367`) |
| `MA+0x26014/0x26018` | reset `FUN_140A71C30` (`0x140A71C92/0x140A71CAB`: `= 0xFF`); per-tick loop at `0x1404573B7-0x14045747B` writes `MA+0x26014[i]` from the `MA+0x2470C` table scan (decompile tail) | pair slots (status INF: values from NearestList-order table) |
| `MA+0x24838` | (not re-traced here) | tracked holder id, read beside the carrier at `0x140701993`, `0x1409DFE49` |
| `MA+0x2483C` | setter `FUN_140A72A60` (foundation E5c, VERIFIED) | the holder: `*(p+0x2483C)=param_2` conditional |
| `MA+0x24840` | writer OPEN (foundation B3) | per-team 11×{id, arrival} lists |

## §6 Port spec (HolderSplit cutover)

GF today: `ballRetainer` (`match.hpp:596`, `Get/SetBallRetainer`), set at `humanoid.cpp:1227` (keeper catch), `:1333` (deflect hold), `:1345` (latch), `:1933`; cleared at `:1387` (retain block), `match.cpp:502`.
PES: one u8/dword slot `MA+0x2601C`, values 0..21 or `0xFF`; sole live writer W1 (newest ring-1 key actor or `0xFF`), sole reset W2 (`0xFF` at init); validity `< 22` (`0x16`); reads fall back carrier → holder (`+0x2483C`) → `T+0x78` (`FUN_140A63020`, the exact 3-level rule).

1. Keep the GF symbol `ballRetainer` (it already names this slot); change its clear rule: PES has NO clear-on-loss writer, so the retain-block clear (`humanoid.cpp:1387`) is deleted on the cutover and replaced by W1's rule — recompute the retainer from the newest touch key each tick (`CarrierFromTouch`, `pesreceive.hpp:244`, already the W1 shape: toucher or `0xFF`), not from ball distance. Until the K=8 key stream exists, `GetLastTouchPlayer` stays the stand-in (ponytail already names `G+0x2A80`).
2. Add the `FUN_140A63020` 3-level fallback (carrier → holder → `T+0x78`) at the GF consumers that today read only the retainer (`elizacontroller.cpp` dodge gate, `team.cpp:481`, `match.cpp:888`): `GetBallRetainer()` else `GetDesignatedPossessionPlayer()` else `T+0x78`-equivalent. The holder setter (`FUN_140A72A60`) and the `T+0x78` writer stay OPEN inputs; name both.
3. Latch timing: W1 runs in the listener step-5 path (`FUN_140426760 ← FUN_1404582C0 ← FUN_140458880 ← FUN_140457070`), i.e. AFTER the event stage and BEFORE/AFTER the team tick `FUN_1404F6C30` (placement vs the 10-step tick OPEN, foundation H3). GF's latch (`humanoid.cpp:1343`) runs in the humanoid step. Order PES-first: recompute-then-latch per tick; record the order as INF until H3 closes.
4. Step-43 sweep: keep the `T+0x218==0` gate exactly (`pes-port-TeamT2.md` run 6-8); it takes no carrier input, so a stale last-toucher carrier never blocks a release. No change needed there beyond what TeamT2 already ports.

## §7 P+0x19BA (mirror-cycle flag, bonus item)

- Reader CONFIRMED: `0x1406EC700: 0F B6 81 BA 19 00 00 C3` = `MOVZX EAX,byte ptr [RCX+0x19BA]; RET`. It is `vt+0x30` of the `+0xAB8` action class (vtable `0x1425E3508+0x30` = `0x1406EC700`; single absolute-qword ref at file `0x25E3538` CONFIRMED).
- Writer: OPEN. True-displacement scan for `0x19BA` finds 6 hits, exactly one a memory read (above); the other five are immediates (`ADD/OR EAX,0x19BA`). Opcode-anchored sweep for `C6/88/C7/89 [..+0x19BA]` stores and `8D lea [..+0x19BA]` finds ZERO hits anywhere in the 462 MB image. So the flag is never stored with a direct disp32/disp8 to `+0x19BA`: it is written through a struct copy, a base-adjusted split (e.g. `[reg+0x19B0]+0x0A` — sweep over disp32 `0x1900-0x1A00` writes timed out at 120 s, NOT completed), or a pointer-derived base. Next: finish that sweep in chunks, or xref the vtable slot's owner object.
- GF: `ClipFlag()` stays `false` with its ponytail (`pesmovestep.cpp:396`); the 4-cycle mirror map (`{0→2,1→3,2→0,3→1}`, step-rate §1) is already decoded and needs no new state. Message to ClipRoot: NOT sent (no `agent://` channel in this session; deliver via this report).

## §8 Status and open items

| claim | status | evidence |
|---|---|---|
| 2 writers (W1 live, W2 reset), no per-tick clear | CONFIRMED | §1 opcodes + counts; checker |
| value = newest ring-1 key actor or 0xFF; validity `< 22` | CONFIRMED | `FUN_140457070` lines 129-150; `CMP …,0x15` at 12 sites |
| no clear on loose/deflect/stoppage/set-piece | CONFIRMED | flat scan: only W2 writes 0xFF |
| reader battery (19 reads, incl. `FUN_140A63020` 3-level rule) | CONFIRMED lines / INF sweep placement | §3 table; decompiles in scratch |
| step-43 gate takes no carrier input | CONFIRMED | `0x1404F78DB` gate + `FUN_140A40820` body |
| W1 frame placement (listener step 5) | CONFIRMED path / INF vs team tick | E8 chain + tick-steps3 |
| `TA+0x11` flag caller, `T+0x78` writer, `FUN_1404582C0` caller | OPEN | press-coord §12 items 1-2, first-touch §10 item 4 |
| `0x140802DC9` enclosing function | OPEN | bridge returned empty; asm-only |
| `P+0x19BA` writer | OPEN | §7 (elimination done; chunk sweep left) |
| GF cutover (`CarrierFromTouch` per-tick recompute; delete distance clear) | SPEC (not implemented; read-only brief) | §6 |

Checker: `<workdrive>/gf-HolderRE-scratch/check_holder.py` — CLEAN (46 rows: 28 constants + 18 asm/scan rows); mutations 3/3 fire (float 45→46, W1 opcode byte, disp count 21→22).
