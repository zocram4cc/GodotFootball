# PES port: SepLane1 (PES's per-man contact push, FUN_1408C48C0). NOTHING LANDED: decode, blocker, rig requests

Branch SepLane (worktree <workdrive>/gf-SepLane), base origin/Integrate e154ed687 (= WalkLane 66f2930d7 merged). Tip: e154ed687 (no commit; nothing pushed, see section 7). Scenario claimed: none.

## 0. Rig requests (Main: hook VA + predicate + what each capture must show)

Blocker: the per-man push (`FUN_1408C48C0`), its gather (`FUN_1408C31C0`) and the share (`FUN_140A6D670`) have ZERO captures on disk (`live2/*/index.json` hook names: none of 1408C48C0 / 1408C31C0 / 140A6D670). Without them no PES-proven red test or bit-exact record can be built, so no GF change can meet M1 or M2. Requested, one PES restart (batched):

| # | Hook (VA) | Predicate (rig syntax) | Cap / keep | What the capture must show |
|---|---|---|---|---|
| R1 | `contact_push_1408C48C0` `0x1408C48C0` (entry, rcx = P) | `EVERY 20`, `--when 'd(q(q(0x1436F3DC8)+0x270)+0x116c)==1'` (in play) | cap 24 global, FULL, pinned | P's record before the call (`P+0x624/0x62C`, `P+0x6d8` partner record, `P+0x661`, `P+0x948`, `P+0x77d`), ap = `*(P+0x7d8)` and `ap+0x248c..0x2494`, the byte `*(func_0x141b985e0()+0xb9a0)` (the gate, OPEN), and the RETURN ADDRESS (`ret`), which names the live caller (candidates §3.2). Exit state needs a second hook at `0x1408C4CEB`-epilogue or the next entry of the same P. |
| R2 | `contact_gather_1408C31C0` `0x1408C31C0` (entry) | same predicate as R1, cap 12, FULL | the contact-object list of P (`P+0x7f8`, kind 5 and kind 7 entries, their radii and positions), the out-record `param_3` and the vector `param_2` on return (hook the return via `ret` of R1's successor) |
| R3 | `contact_share_140A6D670` `0x140A6D670` (entry) | `--when 'ret in (0x1408C37A2,0x1408C4B5F)'`, cap 36, FULL | args (ctx, P, Q); the returned float (XMM0 at return); the two PC bytes, the two action bytes at `human+1`, the two collision classes `human+0x3f`, the ball owner (`FUN_140A627E0(ctx,0)`) |

Hook lines go through `untranslated_callees.py hooks_for_image` (rule 17-11-26 / 18-11-26) before arming; I did not write a hook line. Cite nothing from R1–R3 until captured.

## 1. What is decoded (VA-cited; verdicts CONFIRMED = raw asm or decompile at the VA; INF = inference; OPEN = blocked)

### 1.1 Who calls the push, and when (static, `tools/dev/ledger/universe.json` callee graph)
- `FUN_1408C48C0` (per man, `P` in rcx) has 4 static callers: `1408BD970` (113 B, AI, translated closure `src/pes/translated/closure/pes_fn_1408BD970.cpp`; the translated body does NOT contain the 48C0 call, so it is in an untranslated branch), `1408BDD80` (AI, untranslated), `1409F1640` (290 B, AI, `pesbodyscan.hpp` / `pesworldrecord.hpp` name it), `140A367C0` (29 B, AI, untranslated). CONFIRMED (graph); INF which one runs per tick (R1's `ret` decides it).
- Chain above them: `14091E9F0` (per-man AI, translated `src/pes/translated/ai/pes_fn_14091E9F0.cpp`, calls `1408BD970` at its line 76) ← `1409211E0` ← `1404187FF` / `14092094C` (AI). CONFIRMED (graph + translated call at line 76).
- Ordering in GF's tick: `Match::CheckHumanoidCollisions` (`src/sim/onthepitch/match.cpp:1378`) runs the AABB pair pushes (bounce at line ~1436; separation at ~1590-1591 inside `CheckHumanoidCollision`), THEN `ProcessPesContacts` (line 1442). PES's order is the reverse of an AABB pass: PES's 48C0 runs per man inside the AI step (above), and its push lands in `P+0x624/0x62c` and the anime root offset. INF that PES's tick runs the AI step before the composer; OPEN until R1's `ret` and an order capture exist.

### 1.2 `FUN_1408C48C0` body (decompile `sep1.txt` in scratch; CONFIRMED structure, two decompiler return values lost)
Gate: `if (*(char*)(func_0x141b985e0() + 0xb9a0) != 0)` (OPEN: what object and byte).
1. `FUN_1408C2760(P, &out, P+0x624)`: if it returns true, the gather is skipped; else `FUN_1408C31C0(P, &out, &rec, &flag)`.
2. `FUN_1408C26D0(P)`, `FUN_140A627E0(P->ap+0x7e0, 0)` vs `P+0x18` (ball owner test); `FUN_140A6E070`, `FUN_140A4C350(..., 9/8/7)` (clip-event stand-ins from the anime layer, untranslated here).
3. `FUN_1408C3D10(P, &rec)` (diffed, see ledger).
4. The clamp: `fVar11 = 10000 / (gs · 3600)` (constants read from the image, §4); the separation components are clamped to `±fVar11` and to `±0.46` (`0x1425AB4AC`) around zero.
5. Writes (CONFIRMED from the store offsets): `P+0x624 += x`, `P+0x62c += z`, `ap+0x248c += x`, `ap+0x2490 += y`, `ap+0x2494 += z`; if `x²+z² < 1e-6` (`0x142577F6C`) the root offset is zeroed (`ap+0x248c`, `ap+0x2494`).
6. `if (flag) { FUN_1404313C0(P+0x72c, P+0x6d8); FUN_1404313C0(P+0x6d8, &rec); P+0x77d = 0 }`, else `P+0x729 = 0; P+0x948 = 0`.
7. The share is only called inside 48C0 when `rec.partner < 0x16` (`0x1408C4B5F`).

### 1.3 `FUN_1408C31C0` (gather, decompile `sep1.txt`; structure CONFIRMED, depth formula OPEN)
- Returns false when `P+0x7f8 == 0` (no contact container).
- Walks the contact object list from `func_0x14081D8E0/8B0/900(P+0x7f8)` (kind 5 PlayerVsPlayer capsules, r = `0x1425E4A34` = 0.405 m, and kind 7 roots). Kind 7 entries: `kind == 7` and `(sub < 3 or sub == 5)` and a 5-bit mask test (`0x8881`), then they fill `param_3` (partner, parts, `+0x1198` frame stamp). Kind 5: an opponent with `IsTeamOf` opposite (team flag test `(P in 11..21) != (Q in 11..21)`), a distance test against `0.405` plus the `0x1425AB4A8` (0.23) offset, a velocity term from `FUN_140A6C6D0(ap, …, 1/gs)` (the 0.6944 `0x14259BDD0` = 2.5 km/h floor, CONFIRMED via `pes-re-contact.md` §1.3), and then `FUN_140A6D670(ctx, P, Q)` (the share) for each opponent pair, with `fStack_1a4 += normal_x · (fVar1 − fVar27) · share` (the `Σ normal·(depth − allowance)·share` of `pes-re-contact.md` §1.2).
- OPEN: `fVar1` (depth) and `fVar27` (allowance `FUN_140A6DD30`, undecoded). Both need the capsule geometry (`FUN_141B970E0` contact callback, `pes-re-collision.md` §2.3-2.4).

### 1.4 `FUN_140A6D670` = the share, FULLY DECODED from raw asm (capstone over `pes_full.bin`; `tools/...` script in scratch `disasm_va.py`). Returns a float in XMM0 (the decompile return is garbage).
Inputs: `P` (edx), `Q` (r8d), ctx (rcx). `hP = human(P)` (r14), `hQ = human(Q)` (rbx), `pP = pl(P)` (r13), `pQ = pl(Q)`.
- `clsP = hP+0x3f`, `clsQ = hQ+0x3f` (signed bytes).
- `teamP = (P ∈ [11,21])`, `teamQ = (Q ∈ [11,21])` (0x140A6D6EB..0x140A6D713).
- Table: `T = f32[0x142648900 + 4·(7·clsP + clsQ)]` (0x140A6D717..0x728). Read values (§4 table).
- If `T < 0` → "general" or the special cases below. If `T ≥ 0`: return `1 − T` if `clsP == 1 && clsQ == 1 && actionQ < 0x5E && bit23(propTable[actionQ] @0x1426484F0)` else return `T` (0x140A6D737..0x76E).
- Special cases when `T < 0` and `teamP == teamQ` (0x140A6D773..0x7A6): `clsP == 6`: return 0 if `clsQ == 0`, else general; `clsP == 0`: return 1.0 if `clsQ == 6`, else general; `clsP == clsQ == 4`: two sub-branches on `hX+0x3C` and `ushort hX+0x1C` against `(4·gs)/10` (0x140A6D7B7..0x7FD), returning 0 or 1.0 or falling to general.
- General (0x140A6D853): `pcP = ability(P)+0x28`, `pcQ = ability(Q)+0x28` (as floats, `cvtdq2ps`).
  - `pcP −= 20` if `actionP == 0x15` and P is not the ball owner (`FUN_140A627E0(ctx,0) != P`) (0x140A6D908..0x912).
  - `pcP −= 15` if `actionP` has property bit 13 (`propTable[actionP] >> 13 & 1`) or `hP+8` bit 16 (0x140A6D920..0x94B); same for Q (0x140A6D957..0x98F).
  - `pcP −= 15` if `clsP == 6`; same for Q (0x140A6D994..0x9A3).
- `diff = pcP − pcQ`. Heading difference `dθ = pQ+0x554 − pP+0x554` wrapped by ±360 when |dθ| > 180 (0x140A6D9C7..0xA06).
- If `|dθ| < 45` (0x140A6DA21): bearings `bP = FUN_1404583B0(pP+0x554, FUN_140474C40(pP+0x55C, pQ+0x55C))` and `bQ` likewise. If `bP − 90 > 45` and `bQ − 90 > 45` (i.e. both > 135°): `diff += +15` if `bQ ≤ bP` else `diff += −15` (0x140A6DA3D..0xAAF). (CONFIRMED against `pes-re-duels-fouls.md` §1.2.)
- `d = clamp(diff, −30, 30) / 30` (0x140A6DAB7..0xAD8; constants 30, −30).
- Speeds: `vP = 3.6 · gs · |velP_2D|` where `velP = pP+0x574/0x57c` minus `+0x64c/+0x654` if `pP+0x661` (contact separation) (0x140A6DB21..0xB4A); same for Q with `vQ`. `sX = clamp((vX − 5) / 8.5, 0, 1)` (constants 5.0 `0x14259222C`, 8.5 `0x1425C48B4`, 13.5 `0x1425A1FFC` as the upper knee) (0x140A6DBCE..0xC3A). `ds = sQ − sP`.
- `share = 0.5 − 0.35·ds − 0.3·(1 − 0.5·|ds|)·d` (0x140A6DC46..0xC98; constants 0.5 `0x142565600`, 0.35 `0x14259D5C8`, 0.3 `0x142590D98`).
- Key-player branch (0x140A6DCA5..0xCEF): when `teamP != teamQ` and the ball owner ∉ {P,Q} and `FUN_140A48A30(G, teamP)+0x78 == P` or `FUN_140A48A30(G, teamQ)+0x78 == Q`: `share = pcQ' / (pcP' + pcQ')` (0x140A6DCDE..0xCE6). INF: `FUN_140A48A30(G, team)` returns the team's key-player record.
- Verdict: CONFIRMED for every branch listed (raw asm); the only inference is the meaning of the `+0x78` key-player record.

### 1.5 Share vs the bounce GF uses
GF's `CheckHumanoidCollision` pushes by `bounceBias` (backfacing ±0.8, velocity ±0.2, tackle 0.1+0.4·stat, possession ±0.4, ball distance ±0.6, Balance ±1.0; `match.cpp:1463-1540`). PES's push uses Physical Contact (`ability+0x28`), the 0.23/0.405 capsule geometry, speed classes, the 45°/135° heading test, and the 7×7 class table. Balance is not in PES's share (CONFIRMED by 1.4). The two are different functions; the bounce is a GF stand-in (`match.cpp:1435` names it).

## 2. Why nothing lands in this run

1. **The gather input is the wrong shape in GF.** `ProcessPesContacts` (`match.cpp:1845`) and `PartsOf` (`match.cpp:1752`) build contact boxes from the humanoid node's render geometry AABBs (`GetHumanoidNode()->GetObjects(e_ObjectType_Geometry)`), not from PES's collision objects. PES's gather (1.3) reads kind 5 capsules (r 0.405) and kind 7 roots from a collision world with a contact mask (`pes-re-collision.md` §2.3-2.4). GF has no such world. A push computed from AABB overlap would be a new stand-in, which rule 8 forbids unless PES's path is proven out of reach. It is not out of reach: it needs the collision world built, which is its own subsystem.
2. **No captures.** Zero captures of 48C0, 31C0 or 6D670 on disk. Without R1–R3 no bit-exact record or red test can be PES-proven.
3. **The share alone is dead code.** Porting 6D670 (1.4) without the gather changes nothing; nothing calls it except 31C0 and 48C0.
4. **M2 is not reachable either**: any push that replaces the bounce would move the number on men driven by GF stand-ins (the bounce itself), and the stand-in lane would need to exist first.

## 3. Facts the next round needs

### 3.1 Tick order (CONFIRMED vs INF)
- CONFIRMED (static graph): 48C0 is reached only through the AI step (`14091E9F0` → `1408BD970`, and three other AI callers). INF: PES's composer `FUN_141AFD400` runs after the AI step (WalkLane10 P2 showed the composer reads B+0x90 while B+0x81 is set; order vs the push is OPEN). R1's `ret` and the composer's capture order decide it.

### 3.2 Candidate callers of 48C0 (static, untranslated except 1408BD970)
`1408BD970` (translated closure, its 48C0 call is in a branch the translation did not reach), `1408BDD80`, `1409F1640`, `140A367C0`. R1's `ret` picks one.

## 4. Constants (read by script from `pes_full.bin`, flat offset = VA − 0x140000000; each paired with its operand VA)

| VA | value | where (instruction VA) |
|---|---|---|
| 0x142565600 | 0.5 | share base, 0x140A6DC46..0xC65 (`movss xmm2,[0x142565600]`) |
| 0x14259D5C8 | 0.35 (0.3499999940) | share ds term, 0x140A6DC5D |
| 0x142590D98 | 0.3 (0.3000000119) | share d term, 0x140A6DC84 |
| 0x14256AE1C | 1.0 | 1−T, share clamp |
| 0x142588218 | −1.0 | heading wrap sign |
| 0x14259BE30 | 45.0 | heading gate, 0x140A6DA0A/DA21 |
| 0x14259BE5C | 90.0 | bearing offset, 0x140A6DA7F |
| 0x14259BE60 | 135.0 | bearing gate (via 90+45) |
| 0x142598580 | 180.0 | heading wrap, 0x140A6D9E2 |
| 0x14259858C | 360.0 | heading wrap, 0x140A6D9FE |
| 0x142591140 | 30.0 | diff clamp, 0x140A6DAB7 |
| 0x14259BE88 | −30.0 | diff clamp low, 0x140A6DACA |
| 0x14259BE0C | 15.0 | bearing bonus, 0x140A6DAAF |
| 0x14259D668 | −15.0 | class/flag penalty, 0x140A6D8FF |
| 0x1425A946C | −20.0 | DODGE penalty, 0x140A6D8F7 |
| 0x14259222C | 5.0 | speed floor, 0x140A6DBCE |
| 0x1425C48B4 | 8.5 | speed span, 0x140A6DBD9 |
| 0x1425A1FFC | 13.5 | speed knee, 0x140A6DBFE |
| 0x14259A764 | 60.0 | km/h factor, 0x140A6DB26 |
| 0x14256AE34 | 1000.0 | km/h divisor, 0x140A6DB52 |
| 0x14259BDD0 | 0.6944444 | 2.5 km/h floor (pes-re-contact §1.3), 48C0/31C0 |
| 0x1425E4A34 | 0.405 | kind-5 capsule radius, 31C0 |
| 0x1425AB4A8 | 0.23 | 31C0 overlap offset |
| 0x1425AB4AC | 0.46 | 48C0 clamp |
| 0x142577F6C | 1e-6 | root-offset zero threshold, 48C0 |
| 0x142598590 | 3600.0 | 48C0 clamp denominator |
| 0x14259D634 | 10000.0 | 48C0 clamp numerator |

Class-pair table `f32[0x142648900 + 4·(7·clsP + clsQ)]`, rows clsP 0..6, cols clsQ 0..6 (script-read):
```
0: [-1.0, 1.0, 0.8, 0.1, 1.0, -1.0, -1.0]
1: [ 0.0, 0.0, 0.0, 0.0, 0.0,  0.0,  0.0]
2: [ 0.2, 1.0,-1.0, 0.1, 1.0,  0.2,  0.2]
3: [ 0.9, 1.0, 0.9, 1.0, 1.0,  0.9,  0.9]
4: [ 0.0, 1.0, 0.0, 0.0,-1.0, -1.0, -1.0]
5: [-1.0, 1.0, 0.8, 0.1,-1.0, -1.0, -1.0]
6: [-1.0, 1.0, 0.8, 0.1,-1.0, -1.0, -1.0]
```
Note: `0x142648900` is the table; the 7×7 index is the class (0..6); row 1 is all 0.0, which is a T ≥ 0 case (returns 0). The `gs` factor is `FUN_141E532D0()` (INF: frames per second, 54 per `pes-re-duels-fouls.md` §1.2 `gs`).

## 5. Measurements on e154ed687 (base, no change)
- `PesStallQuick.ALiveBallNeverLiesStillInTheQuickWindow` (`pes_restart_delay_test --gtest_filter=PesStallQuick.*`, `GF_STALL_DUMP=1`, seed 7, `PES21_ROOT=<workdrive>/PES21 PES_EXE=<pes_full.bin>`, slot via simslot): **PASSED, `forced_live=49`, buckets `armed=0 live=49 unarmed=0`**, 380 s. Matches the bound 49 (WalkLane10).
- `PesStall.ALiveBall*` (bound 479): not run (no behaviour change; the rule for a behaviour change does not apply). The 479 is WalkLane10's measured value on this tip.
- `pes_receive_test`: **59 passed, 1 failed**: `PesAnimeAction.ABallActionIsNeverTakenForMovementOrForTheTrapItself` (already red on the base; WalkLane11 recorded it as RED on 66f2930d7).
- `pes_pass_target_test`: run in background (job bg_521), result below if it finished.

## 6. Gate and mutations
- Build: `pes_restart_delay_test`, `pes_pass_target_test`, `pes_receive_test` (Release, Ninja, ccache). `gen_translated.py --check`: not run (no translated file changed).
- Mutations: none (no code change).
- `GATE fast`: not run (nothing landed; Main gates batches).

## 7. Repo state
- Branch `SepLane` created locally at e154ed687 (worktree `<workdrive>/gf-SepLane`, `extern/godot-cpp` symlinked). NOT pushed: there is no commit to push. origin has no SepLane branch.
- Scratch: `<workdrive>/gf-SepLane-scratch/` (dec/sep1.txt: decompiles of 48C0, 31C0, 6D670; tools/disasm_va.py: capstone + annotated constants, flat image; base_quick.log, base_receive.log, base_passtarget.log).

## 8. Open items (numbered)
1. R1–R3 hooks (section 0). Without them 48C0's exit state, the gather's `param_2/param_3/param_4` and the allowance `FUN_140A6DD30` cannot be proven.
2. Gather depth formula (31C0 `fVar1`, `fVar27`) and `FUN_140A6DD30` (allowance): undecoded. Needs the collision contact callback `FUN_141B970E0` and the kind-5/kind-7 object geometry (`pes-re-collision.md` §2.3-2.4).
3. The collision world (kind 5 capsules, kind 7 roots, contact mask) does not exist in GF. The contact gather cannot be ported on the AABB boxes (`PartsOf`) without a new stand-in. That world is a separate subsystem: name its owner before porting the push.
4. `FUN_141B985E0` and the byte `+0xb9a0` (48C0's gate): undecoded.
5. Which of 4 static callers of 48C0 runs per tick (R1's `ret`).
6. `FUN_140A48A30(G, team)` (key-player record, `+0x78`): meaning INF.
7. The share is decoded in full (1.4); its port needs the gather and R3's return values to be checked against it (red test: a two-man overlap from R3's capture).
8. O2 from WalkLane10 (does a non-striking man's root offset move his root?): still open; the composer `FUN_141AFD400` reads `ap+0x248c` only when B+0x81 is clear (WalkLane10 P3 says no reader found; a full-image scan is still needed).

## 9. Next highest-value step
Main: arm R1–R3 in the next batch (they are cheap: 24 + 12 + 36 captures). Lane: with R1/R2 captures, prove 48C0's exit state bit-exact on the captures (real calls), then port the share (1.4) and the gather's contact-object loop once the collision world owner is named. Do not touch the bounce until the gather is on PES's objects.

## 10. Rig-free facts gained this run (for Main's archive)
- `FUN_140A6D670` return value and branches fully decoded from raw asm (1.4). This fixes the WalkLane11 section-4 formula's special cases (table row 1, the 0x15/bit-13/class-6 penalties, the key-player branch), which WalkLane11 had only from the doc.
- Caller graph of 48C0 (1.1); three of four callers are untranslated AI functions.

## 11. Results appended after the last measurements (base e154ed687, no change)
- `pes_pass_target_test` (Release; simslot; 778 s): `ATrapsRootMovesByExactlyTheStrikeAdjusterStep` OK; `EveryPassInFlightHasItsOwnSidesReceiver` OK; `EveryPassGetEpisodeIsWithinPesLiveRange` RED: `6 of 34 PASS_GET episodes exceeded 2.94 s; longest 11.61 s, median 1.148 s` (this is WalkLane10's O4, unchanged); `IsSetInFlightAndNeverStaleWhileTheBallRests` SKIPPED (named gap, unchanged). Matches WalkLane10 section 3, so the base is reproducible.
- Correction to 1.1: "so it is in an untranslated branch" is INF. The translated body of `1408BD970` lacks the 48C0 call; the reason (branch not reached vs translator omission) is not shown.
- Unrun by design: `PesStall.ALiveBall*` (no behaviour change), `gen_translated.py --check` (no translated file changed), `GATE fast` (nothing landed).
