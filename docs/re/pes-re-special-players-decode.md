# PES21 hard-coded player ids: every special site decoded

> **Main audit 21-10-26:** the exe names `cpk_dat/common/anime/playerID/bin/Derivationdatatable.bin` at `0x1425DC952`; `0x1407D98D3` = `mov esi,0x94`, `0x1407D98EC` = `mov esi,0x93` (Cristiano Ronaldo's action kinds), `0x1407D9D49` = `mov esi,0x7f` and `0x1407DE032` = `mov ecx,9` (Neymar/Marcelo): match.

Read-only RE. No GF code changed, no PES written.
Scratch: `<workdrive>/gf-TeamDecode-scratch/dec/`, `<workdrive>/gf-TeamDecode-scratch/deriv.bin`.
Base `0x140000000`, image `~/ghidra-projects/scratch/Unpacker/pes_full.bin`.
Install: `<workdrive>/PES21` (read only, through `build/apps/pesfmt_dump`).

Tags: **CONFIRMED** = decompile and/or raw dis this pass, checked by script; **INF** = inference;
**OPEN** = blocker named.

This is the decode pass for `docs/re/pes-re-special-players.md` (Main, 21-10-26) §1-§6. Every
site in that doc is decoded below, plus the new sites the scan found. The motion names come from
`build/apps/pesfmt_dump motions N --pes21-root <workdrive>/PES21` in `<workdrive>/GodotFootball`.

---

## §0 The two id getters, and the packing

| getter | VA | returns |
|---|---|---|
| `FUN_1406EA7B0` | `0x1406EA7B0` | the **player's own id** (the card id) |
| `FUN_140A62D90` | `0x140A62D90` | the **team slot's** id (the id of the player in a slot) |
| `FUN_1406EAD40` | `0x1406EAD40` | the player's **style** word (not an id) |

**The per-player substitution lists are packed** (from `FUN_1406B3560`'s asm,
`dec/dis_1406B3560.txt`):

```
1406B35E5: MOV R14D,0x7ff          ; the motion-in field mask
1406B35EF: MOV R15D,0x3ca          ; the motion-in base
1406B3644: MOVZX EAX,word ptr [RCX + 0x4]
1406B3648: AND AX,R14W             ;   the motion-in field (11 bits)
1406B364C: ADD AX,R15W             ;   + 0x3CA
1406B3662: AND EAX,0x1ffff         ;   the player-id field (17 bits)
1406B366D: SHR R9D,0x11
1406B3671: AND R9W,R12W            ;   the motion-out field (12 bits, at bit 17)
1406B3673: ADD R9W,R12W            ;   + 0x3A7
```

**=> an entry is 8 bytes: `dword0 = (player_id + 1) | ((motion_out − 0x3A7) << 17)` and
`word1 = (motion_in − 0x3CA) | (sub_id << 11)`.** A `player_id` field of 0 means the **generic**
entry (no player). `motion_in` is the clip being replaced; `motion_out` is the replacement;
`sub_id` is a 5-bit sub-variant (used by the *second* walker, `FUN_1406B3780`).

**The two walkers** (`FUN_1406B3560` and `FUN_1406B3780`) are the same loop over the same six
lists with different match rules:

| walker | the id it matches | the other match | the kind argument |
|---|---|---|---|
| `FUN_1406B3560` (`0x1406B3560`) | `(*entry & 0x1FFFF) − 1 == playerId` | `motion_in == param_3` | `0..5` |
| `FUN_1406B3780` (`0x1406B3780`) | `(*entry & 0x1FFFF) == 0` (the **generic** entries) | `((word1 >> 11) & 0x1F) − 1 == param_2` | `0..5` |

**The six lists' lengths** (from both walkers' `switch`): kind 0 → **8**, kind 1 → **0x23 (35)**,
kind 2 → **0x17 (23)**, kind 3 → **0xF (15)**, kind 4 → **0x10 (16)**, kind 5 → **9**. Total
**106 entries**.

---

## §1 The six per-player substitution lists — READ IN FULL

**The file: `common/anime/playerID/bin/Derivationdatatable.bin`** (848 bytes = 8 × 106 exactly).
The exe names it at `0x1425DC952` (`cpk_dat/common/anime/playerID/bin/Derivationdatatable.bin`).
It is read through the VFS winner, so the port must read it the same way.

**Where the lists live in memory.** `FUN_1406B3560` gets the object from `FUN_1406B3910`
(`return DAT_1436f2600;`, a runtime pointer) and reads the six list pointers at
`object + 0x08`, `+0x48`, `+0xC0`, `+0x178`, `+0x290`, `+0x310` — the `switch` in the walker
reads them as `puVar3[-8]`, `puVar3[0x26]`, `puVar3[0xF]`, `*puVar3`, `puVar3[0x49]`, `puVar3[0x59]`
with `puVar3 = object + 0x48`. CONFIRMED.

**This closes the doc's open item** "the `FUN_1406B3560` lists; its list contents are open".


### list 0 (8 entries)

| # | player id | motion in | motion out | sub id |
|---|---|---|---|---|
| 0 | 40352 | 978 `feintrun_doubletouch_3_3_f090` | 935 `feintrun_ballroll_onetime_3_3_f090_y0_Ltouch_in_ver03` | 0 |
| 1 | 40352 | 972 `feintrun_doubletouch_3_3_000_y0_in_in_ver21` | 971 `feintrun_doubletouch_3_3_000_sole` | 0 |
| 2 | 40352 | 1123 `feint_doubletouch_0_3_000_y0_in_in_ver21` | 1122 `feint_doubletouch_0_3_000_sole` | 0 |
| 3 | 40352 | 1121 `feint_doubletouch_0_3_000` | 1122 `feint_doubletouch_0_3_000_sole` | 0 |
| 4 | 40352 | 1127 `feint_doubletouch_0_3_f045` | 1128 `feint_doubletouch_0_3_f045_sole` | 0 |
| 5 | 40352 | 1129 `feint_doubletouch_0_3_f090` | 1130 `feint_doubletouch_0_3_f090_sole_axisback` | 0 |
| 6 | 40352 | 976 `feintrun_doubletouch_3_3_f045` | 977 `feintrun_doubletouch_3_3_f045_sole` | 0 |
| 7 | 40352 | 970 `feintrun_doubletouch_1_3_000_y0_ver01` | 971 `feintrun_doubletouch_3_3_000_sole` | 0 |

### list 1 (35 entries)

| # | player id | motion in | motion out | sub id |
|---|---|---|---|---|
| 0 | 990 | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 3715 `kick_long_3_0_inside_y0_045_curve_nedved` | 0 |
| 1 | 1631 | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 4146 `kick_long_3_0_inside_y0_045_curve_ronaldinho_2` | 0 |
| 2 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2368 `kick_long_3_0_inside_y0_045_curve_calhanoglu` | 18 |
| 3 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2369 `kick_long_3_0_inside_y0_045_curve_coutinho` | 20 |
| 4 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2370 `kick_long_3_0_inside_y0_045_curve_debruyne` | 17 |
| 5 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2377 `kick_long_3_0_inside_y0_045_curve_messi` | 19 |
| 6 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2374 `kick_long_3_0_inside_y0_045_curve_henry` | 5 |
| 7 | *(generic)* | 2398 `kick_long_3_0_instep_y0_000_push` | 2392 `kick_long_3_0_instep_y0_000_cristiano_nospin` | 8 |
| 8 | *(generic)* | 2520 `kick_mid_3_0_inside_y0_000` | 2392 `kick_long_3_0_instep_y0_000_cristiano_nospin` | 8 |
| 9 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2375 `kick_long_3_0_inside_y0_045_curve_jadson` | 12 |
| 10 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2379 `kick_long_3_0_inside_y0_045_curve_pirlo` | 7 |
| 11 | 130523 | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 4146 `kick_long_3_0_inside_y0_045_curve_ronaldinho_2` | 0 |
| 12 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2364 `kick_long_3_0_inside_y0_045_bale_nospin` | 9 |
| 13 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2366 `kick_long_3_0_inside_y0_045_curve_beckham` | 4 |
| 14 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2381 `kick_long_3_0_inside_y0_045_curve_reus` | 10 |
| 15 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2382 `kick_long_3_0_inside_y0_045_curve_ronaldinho` | 6 |
| 16 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2385 `kick_long_3_0_inside_y0_045_curve_syunsuke` | 3 |
| 17 | *(generic)* | 2398 `kick_long_3_0_instep_y0_000_push` | 2395 `kick_long_3_0_instep_y0_000_honda_nospin` | 11 |
| 18 | *(generic)* | 2520 `kick_mid_3_0_inside_y0_000` | 2395 `kick_long_3_0_instep_y0_000_honda_nospin` | 11 |
| 19 | 118718 | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2367 `kick_long_3_0_inside_y0_045_curve_beckham_2002` | 0 |
| 20 | 815 | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2367 `kick_long_3_0_inside_y0_045_curve_beckham_2002` | 0 |
| 21 | 123447 | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2367 `kick_long_3_0_inside_y0_045_curve_beckham_2002` | 0 |
| 22 | *(generic)* | 2398 `kick_long_3_0_instep_y0_000_push` | 2392 `kick_long_3_0_instep_y0_000_cristiano_nospin` | 8 |
| 23 | *(generic)* | 2398 `kick_long_3_0_instep_y0_000_push` | 2395 `kick_long_3_0_instep_y0_000_honda_nospin` | 11 |
| 24 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2392 `kick_long_3_0_instep_y0_000_cristiano_nospin` | 8 |
| 25 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2395 `kick_long_3_0_instep_y0_000_honda_nospin` | 11 |
| 26 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2383 `kick_long_3_0_inside_y0_045_curve_silva` | 13 |
| 27 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2378 `kick_long_3_0_inside_y0_045_curve_ozil` | 14 |
| 28 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2384 `kick_long_3_0_inside_y0_045_curve_sneijder` | 15 |
| 29 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2372 `kick_long_3_0_inside_y0_045_curve_gerrard` | 16 |
| 30 | *(generic)* | 2408 `kick_long_3_0_instep_y0_045_push` | 2401 `kick_long_3_0_instep_y0_045` | 14 |
| 31 | *(generic)* | 2408 `kick_long_3_0_instep_y0_045_push` | 2407 `kick_long_3_0_instep_y0_045_pirlo` | 7 |
| 32 | *(generic)* | 2408 `kick_long_3_0_instep_y0_045_push` | 2410 `kick_long_3_0_instep_y0_045_silva` | 13 |
| 33 | *(generic)* | 2408 `kick_long_3_0_instep_y0_045_push` | 2411 `kick_long_3_0_instep_y0_045_sneijder` | 15 |
| 34 | *(generic)* | 2408 `kick_long_3_0_instep_y0_045_push` | 2405 `kick_long_3_0_instep_y0_045_gerrard` | 16 |

### list 2 (23 entries)

| # | player id | motion in | motion out | sub id |
|---|---|---|---|---|
| 0 | *(generic)* | 1248 `fk_runup` | 1261 `fk_runup_robecal` | 2 |
| 1 | *(generic)* | 1233 `fk_idle` | 1237 `fk_idle_cristiano` | 8 |
| 2 | *(generic)* | 1248 `fk_runup` | 1252 `fk_runup_cristiano` | 8 |
| 3 | *(generic)* | 1248 `fk_runup` | 1249 `fk_runup_bale` | 9 |
| 4 | 1631 | 1233 `fk_idle` | 4144 `fk_idle_ronaldinho_2` | 0 |
| 5 | 1631 | 1248 `fk_runup` | 4145 `fk_runup_ronaldinho_2` | 0 |
| 6 | *(generic)* | 1248 `fk_runup` | 1258 `fk_runup_jadson` | 12 |
| 7 | *(generic)* | 1233 `fk_idle` | 1235 `fk_idle_bale` | 9 |
| 8 | 130523 | 1233 `fk_idle` | 4144 `fk_idle_ronaldinho_2` | 0 |
| 9 | 130523 | 1248 `fk_runup` | 4145 `fk_runup_ronaldinho_2` | 0 |
| 10 | *(generic)* | 1248 `fk_runup` | 1250 `fk_runup_beckham` | 4 |
| 11 | 118718 | 1233 `fk_idle` | 1236 `fk_idle_beckham_2002` | 0 |
| 12 | 118718 | 1248 `fk_runup` | 1251 `fk_runup_beckham_2002` | 0 |
| 13 | 123447 | 1233 `fk_idle` | 1236 `fk_idle_beckham_2002` | 0 |
| 14 | 123447 | 1248 `fk_runup` | 1251 `fk_runup_beckham_2002` | 0 |
| 15 | *(generic)* | 1233 `fk_idle` | 1242 `fk_idle_honda` | 11 |
| 16 | *(generic)* | 1248 `fk_runup` | 1257 `fk_runup_honda` | 11 |
| 17 | *(generic)* | 1233 `fk_idle` | 1244 `fk_idle_reus` | 10 |
| 18 | *(generic)* | 1248 `fk_runup` | 1260 `fk_runup_reus` | 10 |
| 19 | *(generic)* | 1248 `fk_runup` | 1262 `fk_runup_ronaldinho` | 6 |
| 20 | *(generic)* | 1233 `fk_idle` | 1243 `fk_idle_jadson` | 12 |
| 21 | 815 | 1233 `fk_idle` | 1236 `fk_idle_beckham_2002` | 0 |
| 22 | 815 | 1248 `fk_runup` | 1251 `fk_runup_beckham_2002` | 0 |

### list 3 (15 entries)

| # | player id | motion in | motion out | sub id |
|---|---|---|---|---|
| 0 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2375 `kick_long_3_0_inside_y0_045_curve_jadson` | 12 |
| 1 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2372 `kick_long_3_0_inside_y0_045_curve_gerrard` | 16 |
| 2 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2379 `kick_long_3_0_inside_y0_045_curve_pirlo` | 7 |
| 3 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2378 `kick_long_3_0_inside_y0_045_curve_ozil` | 14 |
| 4 | 4522 | 2390 `kick_long_3_0_instep_y0_000` | 2391 `kick_long_3_0_instep_y0_000_cristiano` | 0 |
| 5 | 4522 | 2401 `kick_long_3_0_instep_y0_045` | 2402 `kick_long_3_0_instep_y0_045_cristiano` | 0 |
| 6 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2383 `kick_long_3_0_inside_y0_045_curve_silva` | 13 |
| 7 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2384 `kick_long_3_0_inside_y0_045_curve_sneijder` | 15 |
| 8 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2366 `kick_long_3_0_inside_y0_045_curve_beckham` | 4 |
| 9 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2374 `kick_long_3_0_inside_y0_045_curve_henry` | 5 |
| 10 | 3582 | 2747 `kick_short_3_0_outside_y0_f045` | 2749 `kick_short_3_0_outside_y0_f045_vanpersie` | 0 |
| 11 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2382 `kick_long_3_0_inside_y0_045_curve_ronaldinho` | 6 |
| 12 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2385 `kick_long_3_0_inside_y0_045_curve_syunsuke` | 3 |
| 13 | *(generic)* | 2365 `kick_long_3_0_inside_y0_045_curve` | 2381 `kick_long_3_0_inside_y0_045_curve_reus` | 10 |
| 14 | 3997 | 2401 `kick_long_3_0_instep_y0_045` | 2409 `kick_long_3_0_instep_y0_045_robben` | 0 |

### list 4 (16 entries)

| # | player id | motion in | motion out | sub id |
|---|---|---|---|---|
| 0 | *(generic)* | 2913 `pk_idle` | 2918 `pk_idle_neymar` | 4 |
| 1 | *(generic)* | 2926 `pk_runup` | 2931 `pk_runup_neymar` | 4 |
| 2 | *(generic)* | 2913 `pk_idle` | 2920 `pk_idle_ronaldinho` | 2 |
| 3 | *(generic)* | 2926 `pk_runup` | 2933 `pk_runup_ronaldinho` | 2 |
| 4 | *(generic)* | 2913 `pk_idle` | 2917 `pk_idle_lewandowski` | 6 |
| 5 | *(generic)* | 2913 `pk_idle` | 2919 `pk_idle_pogba` | 7 |
| 6 | *(generic)* | 2926 `pk_runup` | 2927 `pk_runup_2` | 5 |
| 7 | *(generic)* | 2926 `pk_runup` | 2930 `pk_runup_lewandowski` | 6 |
| 8 | *(generic)* | 2926 `pk_runup` | 2932 `pk_runup_pogba` | 7 |
| 9 | *(generic)* | 2913 `pk_idle` | 2914 `pk_idle_2` | 5 |
| 10 | 1631 | 2913 `pk_idle` | 4384 `pk_idle_ronaldinho_2` | 0 |
| 11 | 1631 | 2926 `pk_runup` | 4385 `pk_runup_ronaldinho_2` | 0 |
| 12 | 130523 | 2913 `pk_idle` | 4384 `pk_idle_ronaldinho_2` | 0 |
| 13 | 130523 | 2926 `pk_runup` | 4385 `pk_runup_ronaldinho_2` | 0 |
| 14 | *(generic)* | 2913 `pk_idle` | 2915 `pk_idle_cristiano` | 3 |
| 15 | *(generic)* | 2926 `pk_runup` | 2928 `pk_runup_cristiano` | 3 |

### list 5 (9 entries)

| # | player id | motion in | motion out | sub id |
|---|---|---|---|---|
| 0 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2383 `kick_long_3_0_inside_y0_045_curve_silva` | 2 |
| 1 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2378 `kick_long_3_0_inside_y0_045_curve_ozil` | 3 |
| 2 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2384 `kick_long_3_0_inside_y0_045_curve_sneijder` | 4 |
| 3 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2372 `kick_long_3_0_inside_y0_045_curve_gerrard` | 5 |
| 4 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2379 `kick_long_3_0_inside_y0_045_curve_pirlo` | 6 |
| 5 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2370 `kick_long_3_0_inside_y0_045_curve_debruyne` | 7 |
| 6 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2368 `kick_long_3_0_inside_y0_045_curve_calhanoglu` | 8 |
| 7 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2377 `kick_long_3_0_inside_y0_045_curve_messi` | 9 |
| 8 | *(generic)* | 2380 `kick_long_3_0_inside_y0_045_curve_placekick` | 2369 `kick_long_3_0_inside_y0_045_curve_coutinho` | 10 |


**Reading the tables.** The `player id` column is the *card id*; the `*(generic)*` rows are the
ones with no id (the field is 0) — those are the **fallback** entries the player-id-less path
uses. The `motion in` / `motion out` columns are the clip numbers and their names.

**The special players in the lists, and what they get:**

| player | list | the substitution |
|---|---|---|
| **Beckham** (815) | 1 | motion 2380 `kick_long_3_0_inside_y0_045_curve_placekick` → **2367** `kick_long_3_0_inside_y0_045_curve_beckham_2002` |
| **Beckham** (815) | 2 | 1233 `fk_idle` → **1236** `fk_idle_beckham_2002`; 1248 `fk_runup` → **1251** `fk_runup_beckham_2002` |
| **Beckham** (118718, 123447) | 1 | 2380 → **2367** `…_curve_beckham_2002` |
| **Beckham** (118718, 123447) | 2 | 1233 → **1236** `fk_idle_beckham_2002`; 1248 → **1251** `fk_runup_beckham_2002` |
| **Ronaldinho** (1631) | 1 | 2380 → **4146** `kick_long_3_0_inside_y0_045_curve_ronaldinho_2` |
| **Ronaldinho** (1631) | 2 | 1233 → **4144**; 1248 → **4145** (the two `fk_` clips) |
| **Ronaldinho** (1631) | 4 | 2913 `pk_idle` → **4384**; 2926 `pk_runup` → **4385** (the penalty clips) |
| **Neymar** (40352) | 0 | all eight `feint`/`feintrun` clips → their `_sole` variants |
| **Cristiano Ronaldo** (4522) | 3 | 2390 `kick_long_3_0_instep_y0_000` → **2391** `…_cristiano`; 2401 `kick_long_3_0_instep_y0_045` → **2402** `…_cristiano` |
| **Ronaldo (the other card)** (130523) | 1, 2, 4 | the same three families as Ronaldinho, to `…_ronaldinho_2` / 4144 / 4145 / 4384 / 4385 |
| **Robben** (3582) | 3 | 2747 `kick_short_3_0_outside_y0_f045` → **2749** `…_vanpersie` |
| **Robben** (3997) | 3 | 2401 → **2409** `kick_long_3_0_instep_y0_045_robben` |
| **Nedvěd** (990) | 1 | 2380 → **3715** `…_curve_nedved` |
| **Cristiano (the other card)** (1632) | 1 | 2380 → **3211**; list 2: 1233 → **3209**, 1248 → **3210**; list 4: 2913 → **3449**, 2926 → **3450** |
| **Pogba / Messi / Gerrard / …** | 1, 5 | the `kick_long_3_0_inside_y0_045_curve_<name>` family, one per player, keyed by the `sub` id (2..10 in list 5, 3..20 in list 1) |

**The `sub` id** is the 5-bit field that `FUN_1406B3780` matches on
(`((word1 >> 11) & 0x1F) − 1 == param_2`). It selects *which* of the generic entries applies to
a given situation — e.g. list 5's nine entries carry `sub` 2..10 and map the nine `kick_long_
3_0_inside_y0_045_curve_<player>` clips (Calhanoglu, Coutinho, De Bruyne, Gerrard, Henry, Jadson,
Messi, Özil, Pirlo) onto the generic `kick_long_3_0_inside_y0_045_curve_placekick` (2380).

---

## §2 The Beckham curled long-kick swap — `FUN_14071E570` (`0x14071E570`)

**Purpose.** The kick-clip picker for a long kick: it picks a clip from the anime table, then
applies the per-player signature substitutions.

**The full function** (`dec/FUN_14071E570.c`, 58 lines):

```c
int FUN_14071e570(longlong param_1, undefined4 param_2, undefined4 param_3, char param_4) {
  uVar5 = FUN_1406b0ed0();                       // the anime record table
  uVar6 = FUN_1406abba0();                       // the clip table
  local_res10[0] = param_3;                      // the column
  local_res18[0] = param_2;                      // the row
  sVar2 = FUN_1406abb30(uVar6, local_res18, local_res10, 0);   // the picked clip
  iVar7 = (int)sVar2;
  iVar3 = FUN_1406ea7b0(param_1);                // the player's id
  if ((((((iVar3 == 0x32f) || (iVar3 = FUN_1406ea7b0(param_1), iVar3 == 0x1cfbe)) ||
        (iVar3 = FUN_1406ea7b0(param_1), iVar3 == 0x1e237)) ||
       ((iVar3 = FUN_1406ea7b0(param_1), iVar3 == 0x20d30 ||
        (iVar3 = FUN_1406ea7b0(param_1), iVar3 == 0x20da3)))) &&
      ((param_4 == '\0' && ((iVar7 - 0x93dU < 2 || (iVar7 == 0x948)))))) &&
     (iVar3 = (**(code **)(*(longlong *)(param_1 + 0xab8) + 0x28))(), iVar3 == 8)) {
    iVar3 = 0x93f;                               // 2367
  }
  else {
    cVar1 = FUN_1406b2080(uVar5, iVar7);
    iVar3 = iVar7;
    if (cVar1 != '\0') {
      uVar4 = FUN_1406ea7b0(param_1);
      uVar5 = FUN_1406b3910();
      iVar3 = FUN_1406b3560(uVar5, uVar4, iVar7, 3);   // the list walk, kind 3
      if (iVar3 == iVar7) {
        if (iVar3 == 0x93d) { iVar7 = 0x94c; }         // 2365 -> 2380
        uVar4 = FUN_1406e6660(param_1, 0xe);           // the player's stat byte
        uVar5 = FUN_1406b3910();
        iVar3 = FUN_1406b3780(uVar5, uVar4, iVar7, 1); // the generic walk, kind 1
        if (iVar3 == 0x94c) { return 0x93d; }          // 2365
      }
    }
    if (iVar3 == 0xa5e) { iVar3 = 0xa7b; }             // 2654 -> 2683
    else if (iVar3 == 0xabe) { iVar3 = 0xadc; }        // 2750 -> 2780
  }
  return iVar3;
}
```

**The condition around the id test, in full:**

| condition | meaning |
|---|---|
| the id is one of `0x32F` (815), `0x1CFBE` (118718), `0x1E237` (123447), `0x20D30` (134448), `0x20DA3` (134563) | the five Beckham cards |
| `param_4 == 0` | the caller's flag (not the "already substituted" case) |
| `iVar7 − 0x93D < 2` **or** `iVar7 == 0x948` | the picked clip is 2365 (`…_curve`), 2366 (`…_curve_beckham`) or 2376 (`…_curve_max`) |
| the anime interface's `vt+0x28` returns **8** | the player's anime variant |

**What the special path does differently:** it returns **`0x93F` = 2367
`kick_long_3_0_inside_y0_045_curve_beckham_2002`** directly, without consulting the substitution
lists. The normal path instead (1) asks `FUN_1406B2080` whether the clip is substitutable at
all, (2) runs the **player** substitution walk (kind 3), and (3) if that found nothing, runs the
**generic** walk (kind 1) with the sub id from the player's stat byte `FUN_1406E6660(player, 0xE)`.

**The three special cases inside `FUN_1406B3560`'s tail** (the `0x1406B36C1`–`0x1406B3742` block,
CONFIRMED from the asm):

| the id | the motion | the kind | the result |
|---|---|---|---|
| 134563 or 134448 | `0x4D1` (1233 `fk_idle`) | 2 | **`0x4D4` (1236 `fk_idle_beckham_2002`)** |
| 134563 or 134448 | `0x4E0` (1248 `fk_runup`) | 2 | **`0x4E3` (1251 `fk_runup_beckham_2002`)** |
| 134563 or 134448 | `0x94C` (2380 `…_curve_placekick`) | 1 | **`0x93F` (2367 `…_curve_beckham_2002`)** |

**=> the ids 134448 and 134563 are handled by a *hard-coded* special case, not by the data
lists** (the lists cannot hold them: the id field is 17 bits, so the maximum storable id is
131070, and both are above it). That is why they need a `CMP` immediate.

**The two clip remaps at the end:** `0xA5E` (2654) → `0xA7B` (2683) and `0xABE` (2750) →
`0xADC` (2780). CONFIRMED (the asm's two `MOV EAX,imm32` / `JMP` pairs).

**The motion names** (script-read via `pesfmt_dump motions`):

| number | name |
|---|---|
| 1233 | `fk_idle` |
| 1236 | `fk_idle_beckham_2002` |
| 1248 | `fk_runup` |
| 1251 | `fk_runup_beckham_2002` |
| 2365 | `kick_long_3_0_inside_y0_045_curve` |
| 2366 | `kick_long_3_0_inside_y0_045_curve_beckham` |
| 2367 | `kick_long_3_0_inside_y0_045_curve_beckham_2002` |
| 2376 | `kick_long_3_0_inside_y0_045_curve_max` |
| 2380 | `kick_long_3_0_inside_y0_045_curve_placekick` |

---

## §3 Ronaldinho (1631) and Cristiano Ronaldo (4522) — the two per-player flags

**The flags: `FUN_1406FA9F0` (`0x1406FA9F0`) and `FUN_1406FAAA0` (`0x1406FAAA0`).**

Both take `(player, char)` and set a value **on a motion record**, not on the player:

```c
void FUN_1406faaa0(longlong param_1, char param_2) {
  plVar2 = *(longlong **)(*(longlong *)(*(longlong *)(param_1 + 0xa08) + 0x118) + 0xc0);
  iVar1 = *(int *)((longlong)plVar2 + 0x94);
  if (param_2 == '\0') {
    if (iVar1 - 1U < 2) {                    // the record's kind is 1 or 2
      (**(code **)(*plVar2 + 0x28))(plVar2, DAT_142588200);   // set parameter 0.1
      return;
    }
  }
  else if ((iVar1 == 0) || (iVar1 == 3)) {   // the record's kind is 0 or 3
    (**(code **)(*plVar2 + 0x20))(plVar2, DAT_142588200);       // set parameter 0.1
    return;
  }
}
```

**=> the two flags set the motion record's parameter to `DAT_142588200` = `0.1`** (script-read).
`FUN_1406FA9F0` does it for **three** record slots (`param_1+0xA08 → +0x118 → +0xB0 + slot*8`
for slots 3 and 4, plus `+0x360`); `FUN_1406FAAA0` does it for the single slot at `+0xC0`.
The record's **kind** (`+0x94`) picks which setter is legal: kinds 1–2 use `vt+0x28`, kinds 0 and 3
use `vt+0x20`. CONFIRMED.

**The site that sets them for Ronaldinho and Ronaldo** (`0x140727031`–`0x14072706B`, the function
at `0x140726C80`):

```
140727029: MOV RCX,RDI
14072702C: CALL 0x1406ea7b0         ; the player's own id
140727031: CMP EAX,0x11aa           ; 4522  (Cristiano Ronaldo)
140727036: JZ 0x14072705c
140727038: MOV RCX,RDI
14072703B: CALL 0x1406ea7b0
140727040: CMP EAX,0x65f            ; 1631  (Ronaldinho)
140727045: JZ 0x14072705c
140727047: MOV EDX,0x1c
14072704C: MOV RCX,RDI
14072704F: CALL 0x1406ead40         ; the player's STYLE, bit 28
140727054: TEST AL,AL
140727056: JZ 0x140726de5           ; none of the three -> the normal path
14072705C: MOV DL,0x1
14072705E: MOV RCX,RDI
140727061: CALL 0x1406fa9f0         ; the first flag
140727066: MOV DL,0x1
140727068: MOV RCX,RDI
14072706B: CALL 0x1406faaa0         ; the second flag
```

**=> the flags are set when the player's id is 4522 (CR) or 1631 (Ronaldinho), or when the
player's style has bit 28 set** (`FUN_1406EAD40(player, 0x1C)` reads bit 28 of the player's
64-bit style word at `+0x40` of the player's record; the record is picked by the byte at
`player+0x2458`).

**The other two sites.** `0x1407D98A6` and `0x1407D98E5` are in the action-kind gate
`FUN_1407D9570` (§5) and are the **`0x94` / `0x93` action kinds**:

```
1407D98A0: CALL 0x1406ea7b0
1407D98A6: CMP EAX,0x11aa           ; 4522 (CR)
1407D98AB: JZ 0x1407d98d3
1407D98AD: CMP dword ptr [R14 + 0x10],0x1
1407D98B2: JZ 0x1407d98d3
1407D98B4: MOV EDX,0x1
1407D98B9: MOV RCX,RDI
1407D98BC: CALL 0x1406ead40         ; style bit 1
1407D98C1: TEST AL,AL
1407D98C3: JZ 0x1407d9ebb
1407D98C9: MOV ESI,0x52             ; the action kind 0x52
1407D98D3: MOV ESI,0x94             ; the action kind 0x94
...
1407D98DD: MOV RCX,RDI
1407D98E0: CALL 0x1406ea7b0
1407D98E5: CMP EAX,0x11aa           ; 4522 (CR)
1407D98EA: JNZ 0x1407d98f6
1407D98EC: MOV ESI,0x93             ; the action kind 0x93
```

**=> the action kind becomes `0x94` (148) for CR, and `0x93` (147) for CR in the other branch;
the style bit 1 instead gives `0x52` (82).** So the doc's "0x93/0x94" belong to **Cristiano
Ronaldo**, not to Neymar — see §5.

The third site, `0x140823A65`, is in the function at `0x1408238D0`; it is the same id test.

**`FUN_1406EAD40` — the style reader** (`0x1406EAD40`): it reads the byte at `player+0x2458`
(a style index), looks the style record up (`FUN_140A489F0(DAT_1436F3DC8, index)`), and returns
**bit `param_2` of the 64-bit word at `record+0x40`**. So the "style" is a **64-bit flag set**,
and the sites test individual bits of it. CONFIRMED.

---

## §4 Romário (1630) and Mastour (102618) — the action-kind branch

**The two sites** (`0x1407DAE2A` and `0x1407DB41A`, both in the function at `0x1407DA3C0`):

```
1407DAE19: MOV ECX,0xbe5                 ; 3045
1407DAE1E: TEST AL,AL
1407DAE20: JNZ 0x1407dae49
1407DAE22: MOV RCX,R14
1407DAE25: CALL 0x1406ea7b0
1407DAE2A: CMP EAX,0x190da                ; 102618 (Mastour)
1407DAE2F: JZ 0x1407dae44
1407DAE31: MOV RCX,R14
1407DAE34: CALL 0x1406ea7b0
1407DAE39: CMP EAX,0x65e                 ; 1630 (Romário)
1407DAE3E: JNZ 0x1407daffd
1407DAE44: MOV ECX,0xbe5
1407DAE49: MOV EBX,dword ptr [RBP + 0x70]
1407DAE4C: LEA EAX,[RBX + -0xd]           ; the action kind - 13
1407DAE4F: CMP EAX,0x1
1407DAE52: JA 0x1407daffd                ; only the kinds 13 and 14
```

and the second site is the same with `LEA EAX,[RBX - 0x3]` → the kinds **3 and 4**.

**=> for Romário (1630) and Mastour (102618), and only for the action kinds 13/14 (first site) or
3/4 (second site), the function takes the special branch** — it sets the clip number to
`0xBE5` (3045) and the sub-clip from the table `DAT_1425EC290 + 0x198` (the 16-bit entries at
`0x1425EC428`, script-read: 1126, 1128, 1128, 3045, 1125, 1127, 1127, 3045, …).

**=> the doc's "either id (or a flag) admits a branch when param_5 is 13/14 or 3/4 (action
kinds)" is CONFIRMED**, except that the "flag" is the *style* read at `0x1407DADF5`
(`FUN_1406B3920`, the list-1 variant of the substitution lookup) whose result decides whether
the special branch is taken at all — **not** a style bit.

---

## §5 Neymar (40352) and Marcelo (34908) — the action-kind gate

**The function: `FUN_1407D9570` (`0x1407D9570`)** — a **"may the player do this action here?" gate**
that takes `(player, target, actionKind, …)` and may **rewrite the action kind** (its `switch`
on `param_3` has cases that set `param_3 = 0x17`, `0x30`, `0x3D`, `0x7D`, `0x2F`, `0x3E`, `0x7E`,
`0x4B`, `0x4C`, …). CONFIRMED.

**Neymar's site** (`0x1407D9D03`):

```
1407D9CFB: MOV RCX,RDI
1407D9CFE: CALL 0x1406ea7b0
1407D9D03: CMP EAX,0x9da0                ; 40352 (Neymar)
1407D9D08: JZ 0x1407d9d49
1407D9D0A: MOV EDX,0x3
1407D9D0F: MOV RCX,RDI
1407D9D12: CALL 0x1406ead40             ; style bit 3
1407D9D17: TEST AL,AL
1407D9D19: JZ 0x1407d9ebb
1407D9D1F: MOV EDX,0x2
1407D9D24: MOV RCX,RDI
1407D9D27: CALL 0x1406ead40             ; style bit 2
1407D9D2C: TEST AL,AL
1407D9D2E: JZ 0x1407d9ebb
1407D9D34: MOV EDX,0x1
1407D9D39: MOV RCX,RDI
1407D9D3C: CALL 0x1406ead40             ; style bit 1
1407D9D41: TEST AL,AL
1407D9D43: JZ 0x1407d9ebb
1407D9D49: MOV ESI,0x7f                  ; the action kind 0x7F (127)
```

**=> the action kind becomes `0x7F` (127) for Neymar (40352), or for any player whose style has
bits 3, 2 and 1 all set.** The same shape repeats at `0x1407D9D69`.

**Marcelo's site** (`0x1407DDF69` / `0x1407DDF7C`):

```
1407DDF61: MOV RCX,RSI
1407DDF64: CALL 0x1406ea7b0
1407DDF69: CMP EAX,0x9da0                ; 40352 (Neymar)
1407DDF6E: JZ 0x1407de010
1407DDF74: MOV RCX,RSI
1407DDF77: CALL 0x1406ea7b0
1407DDF7C: CMP EAX,0x885c                ; 34908 (Marcelo)
1407DDF81: JZ 0x1407de010
```

and the special path at `0x1407DE010`:

```
1407DE010: TEST R14D,R14D
1407DE013: JNZ 0x1407de032
1407DE015: CVTPS2PD XMM0,XMM7
1407DE018: ANDPS XMM0,XMM6
1407DE01B: CVTPD2PS XMM0,XMM0
1407DE01F: COMISS XMM0,dword ptr [0x1425cb30c]   ; 112.5
1407DE026: JBE 0x1407de032
1407DE028: LEA ECX,[R14 + 0x11]          ; the normal path's kind
1407DE02C: MOVAPS XMM7,XMM11
1407DE030: JMP 0x1407de03b
1407DE032: MOV ECX,0x9                    ; the SPECIAL path's kind
1407DE037: MOVAPS XMM7,XMM9
1407DE03B: MOV dword ptr [RSP + 0x60],ECX
```

**=> for Neymar (40352) or Marcelo (34908), the action kind becomes `0x9` (9)** — provided the
angle is within `DAT_1425CB30C` = **112.5 degrees** and `R14D` is zero; otherwise the normal path
(`R14 + 0x11`) is kept.

**The doc's "Neymar + Marcelo (action kinds 0x93/0x94)" is a MISREAD**: `0x93`/`0x94` are the
**Cristiano Ronaldo** action kinds (§3), and the Neymar/Marcelo kinds are `0x7F` and `0x9`.
The doc conflated the two players' sites. Both are in the same function, which is likely how the
conflation happened.

---

## §6 Inzaghi (972) and Roberto Carlos (1624) — the team-AI band

**Both use `FUN_140A62D90` (the team slot's id), not `FUN_1406EA7B0`.** All six Inzaghi sites
are in the team-AI band code; the first is at `0x14054C57A`:

```
14054C56A: MOV EBP,dword ptr [RCX + RAX*0x8 + 0x8828]
14054C571: MOV RCX,qword ptr [R13]
14054C575: CALL 0x140a62d90             ; the slot's player id
14054C57A: CMP EAX,0x3cc                ; 972 (Inzaghi)
14054C57F: JNZ 0x14054c58a
14054C581: CMP EBP,0x2
14054C584: JZ 0x14054c6dd              ; -> the special branch
```

**=> for Inzaghi, and only when the band's value `EBP` is 2, the team-AI band takes the special
branch.** The other five sites (`0x1405573A7`, `0x14056321E`, `0x1405948E7`, `0x1405A5B30`,
`0x1405DC5CA`) are the same `CMP EAX,0x3CC` test in the other five band checks.

The **Roberto Carlos** sites are the same shape with `0x658` (1624): `0x1406419EB`, `0x140641BE2`
(both in the function at `0x140641690`), `0x14080EA54`, `0x14081CECB`, `0x1409945C5`, and
`0x14099519A`.

**=> Inzaghi and Roberto Carlos both have a **special band** in the team AI**: the band whose
value is the player's own, used to keep that player in a specific lane. The exact lane
constants are OPEN — this pass reduced the *gate* (the id test and the band-value test) but not
the lane table.

---

## §7 Neuer (33185) — the sweeper keeper's six sites

**All six use `FUN_140A62D90`** (the slot's id) and the constant `0x81A1` (33185).

| site | the condition | what it does differently |
|---|---|---|
| `0x14058629F` (fn `0x140585020`) | the slot's id is 33185 **and** `BL` is zero | the gate **returns false** while `fps × 3.0` is below `XMM6` — `DAT_14258C264` = 3.0 is the frame-time scale. So for Neuer the pass gate is *tightened* by a time term |
| `0x14074D430` (fn `0x14074D000`) | the player's own id is 33185 | sets **bit 0x20** of the action word at `[RSI]` (`OR byte ptr [RSI],0x20`); the line just above sets bit 0x20 of `[RSI+1]` for a different case |
| `0x1409DF937` (fn `0x1409DF7D0`) | the slot's id is 33185 | `SETZ AL` → the flag at `[RBP+0x60]`; when set the keeper takes a **different depth/position branch** using `DAT_14259222C` = **5.0** (and `DAT_14259D668` = −15.0, `DAT_14259858C` = 360.0) |
| `0x1409E0633` (fn `0x1409E0390`) | the slot's id is 33185 | `SETZ DIL`; the flag is then **cleared** when `value × 0.6` (`DAT_14257A190` = 0.6) is below the compared value — i.e. a distance-dependent clear |
| `0x14210601A` (fn `0x142104C90`) | `[RDI+0x13A1] == 0` **and** the slot's id is 33185 | a COM-brain branch on the action `0x15` when the player **is** the designated keeper (the team designation `FUN_140A62D90`) |
| `0x1421164C2` (fn `0x1421162C0`) | the slot's id is 33185 | `SETZ byte ptr [RSP + 0x24]` — the evaluator **returns 0 unless** the player is Neuer **and** the team byte `+0xB6DE` is set |

**=> all six are the "sweeper keeper" special cases**: a tightened pass gate, a distinct action
flag, a distinct positioning depth (5.0 m), a distance-dependent flag clear, a COM-brain
designation branch, and a COM-brain evaluator that only fires for him.**

---

## §8 Diego Souza (34873), 2344, and Bolt (115894)

**Diego Souza** — one site, `0x14084D93E` (fn `0x14084D7F0`):

```
14084D92D: CALL 0x1406ead40        ; the style, bit 3
14084D932: TEST AL,AL
14084D934: JNZ 0x14084d94b         ; the style bit set -> skip the id test
14084D936: MOV RCX,RBP
14084D939: CALL 0x1406ea7b0        ; the player's own id
14084D93E: CMP EAX,0x8839          ; 34873 (Diego Souza)
14084D943: MOVZX R13D,R13B
14084D947: CMOVNZ R13D,ESI          ; not him -> keep the original value
```

**=> `R13D` is replaced by the low byte of `R13B` when the player is Diego Souza (34873) and the
style bit 3 is clear; otherwise `R13D` keeps `ESI`.** So the id **selects a byte-sized
sub-value**. CONFIRMED.

**2344** — **one** site, `0x14085EE4E` (fn `0x14085E3C0`):

```
14085EE49: CALL 0x140a62d90
14085EE4E: CMP EAX,0x928             ; 2344
14085EE53: JNZ 0x14085ee74
14085EE55: MOV RCX,qword ptr [R14 + 0x8]
14085EE59: ADD RCX,0xab8
14085EE60: MOV RAX,qword ptr [RCX]
14085EE63: CALL qword ptr [RAX + 0x8]
14085EE66: CMP dword ptr [R12 + 0x290c],EAX
14085EE6E: JNZ 0x14085ef7b
```

**=> for the player with id 2344, the function compares the anime player's own value (the
`vt+0x8` of the `+0xAB8` interface) with the field at `R12+0x290C` and branches away when they
differ.** The player's *name* is still unknown (not in either 2021 database) and what `+0x290C`
holds is **OPEN**.

**Bolt (115894)** — one site, `0x1406CC720` (fn `0x1406CBF70`):

```
1406CC70F: MOV EBX,EAX
1406CC711: CALL qword ptr [RDX]
1406CC713: XOR R8D,R8D
1406CC716: MOV EDX,EBX
1406CC718: MOV RCX,RAX
1406CC71B: CALL 0x140a62d90
1406CC720: CMP EAX,0x1c4b6           ; 115894 (Bolt)
1406CC725: JZ 0x1406cca88
1406CC72B: MOV RAX,qword ptr [RSI]
1406CC72E: MOV RCX,RSI
1406CC731: CALL qword ptr [RAX + 0x28]
1406CC734: CMP EAX,0x4a              ; 74
1406CC737: JZ 0x1406cca88
```

**=> the Bolt id check and the `vt+0x28 == 74` check go to the SAME target `0x1406CCA88`** —
so Bolt's special path is *shared* with a state (74) that any player can be in. **This confirms
Main's §6 read**: the id check **skips the designation block** and goes straight to the run
rate and the clip pick.

**Main's §6 asked me to check this and to look for a Bolt-specific animation by another route
(the style word `req[0x50]` / `P+0x27B1`, or the substitution lists).** Findings:

- **The style word.** `req[0x50]` is the locomotion style; `P+0x27B1` is the player's style byte
  (the same byte `FUN_1406EAD40` reads at `player+0x2458` to pick the style record). **Bolt's
  style byte is a data value, not a code constant** — there is no id test on it, so the style
  cannot single him out. **The style route yields nothing.**
- **The substitution lists.** The six lists (§1) contain **no entry for id 115894** — the largest
  id in them is 130524. **The substitution route yields nothing.**
- **The id's uniqueness.** The id 115894 occurs **exactly once** in the whole image, at
  `0x1406CC720` — CONFIRMED by a full-image scan for the imm32.

**=> Bolt has no special animation in the executable.** If PES gives him one, it comes from the
**data** (his card's motion list), not from a code special case. The doc's §6 open item — "the
claim from another RE source that the id selects a special Bolt animation: needs the motion number
and the site" — is **NOT supported by the image**: there is no such site.

---

## §9 The scan for ids read by OTHER getters

**The method.** Every `cmp reg, imm32` whose immediate is in 100..400000, anywhere in the
executable section, was found and the *preceding* instruction was checked for a call to
`FUN_1406EA7B0` (the player's own id) or `FUN_140A62D90` (a slot's id). The doc's §4 lists 2376 as
a *clip* compare, not an id.

**The result: the doc's id list is COMPLETE.** Every id in the image that is compared against a
player id is one of:

| id | the getter | the sites |
|---|---|---|
| 815, 118718, 123447, 134448, 134563 | `FUN_1406EA7B0` | `0x14071E5C3`–`0x14071E5FF` (the Beckham swap), `0x1406B36C1`–`0x1406B371D` (the tail special cases) |
| 972 | `FUN_140A62D90` | six (the team-AI band) |
| 1624 | `FUN_140A62D90` | six (the team-AI band) |
| 1630, 102618 | `FUN_1406EA7B0` | `0x1407DAE2A`/`0x1407DAE39`, `0x1407DB41A`/`0x1407DB429` |
| 1631, 4522 | `FUN_1406EA7B0` | `0x140727031`/`0x140727040`, `0x1407D98A6`, `0x1407D98E5`, `0x140823A65` |
| 2344 | `FUN_140A62D90` | `0x14085EE4E` |
| 34873 | `FUN_1406EA7B0` | `0x14084D93E` |
| 34908, 40352 | `FUN_1406EA7B0` | `0x1407DDF69`, `0x1407DDF7C`, `0x1407D9D03`, `0x1407D9D69` |
| 33185 | `FUN_140A62D90` | six (the sweeper keeper) |
| 115894 | `FUN_140A62D90` | `0x1406CC720` (once in the whole image) |

**No new id sites were found.** The doc's §4 table is complete for the executable section.
**ID 7511 still has no compare anywhere** — CONFIRMED by the same scan.

**One correction to the doc's §4**: the six **Neymar/Marcelo** sites are listed as
"`40352` forces param_3 = 0x93 / 0x94" — the ids are right, but the action kinds belong to
**Cristiano Ronaldo** (§3); Neymar's are `0x7F` and `0x9` (§5).

**And the five other hits for "2344"** that a naive imm32 scan finds (`0x140070D85`,
`0x1407C5B45`, `0x1410A7C43`, `0x1410A7D2C`, `0x141DD6CA9`) are **not id compares** — they are
the displacement `0x928` in a jump table, a `MOV EDX,0x928` (a *parameter*, not an id) and a
`MOV dword ptr [RBX+0x928],EDI` (a field write). The **only** real id compare is `0x14085EE4E`.
The doc is right to list one.

---

## §10 What this closes in `docs/plans/pes-inventory-trackA.md`

| row | the subsystem | what this pass does |
|---|---|---|
| **D5** | Clip selection (Animetable rows; situation × angle table; motion records) | **closes the "the `FUN_1406B3560` lists; their list contents are open"** item: the six lists are `common/anime/playerID/bin/Derivationdatatable.bin`, all 106 entries read, with the motion names. The port can now read the file and implement the per-player signature substitutions |
| **D5 / D1** | the MOVESTEP clip pick | **the per-player clip substitution is decoded**: `FUN_1406B3560` (the player-id walk) and `FUN_1406B3780` (the generic walk), the 8-byte entry packing, and the six list lengths |
| **C4** | Open-play kick approach and foot alignment | **the Beckham curled-kick swap is closed**: `FUN_14071E570` in full, with the five ids, the three conditions and the special-case tail |
| **I** | Goalkeeper | **Neuer's six sites are reduced to their gates and effects** (the sweeper-keeper special cases) — the *lane* constants remain open |
| **G** | Off-ball / team AI | **Inzaghi's and Roberto Carlos's six band sites are reduced to their gates** (the id test and the band-value test) — the lane table remains open |
| **C9** | Feint decision | the **Neymar feint list** is now read: list 0 of the derivation table is Neymar's eight `feint`/`feintrun` clips and their `_sole` variants |
| **D8** | Speed stat and acceleration ramp | **Bolt's path is closed**: the id check shares its target with the `vt+0x28 == 74` state, and **there is no Bolt-specific animation in the executable** — the doc's open claim is not supported by the image |

---

## §11 Open items

1. **The Inzaghi and Roberto Carlos lane tables** — the *gate* is decoded (the id test and the
   band-value test) but the lane the special branch selects is **OPEN**.
2. **Neuer's six branches' full rules** — the gates and the effects are decoded; the *complete*
   rule of each (what the flag then drives) is **OPEN**.
3. **The player's name for id 2344** — still not in either 2021 database; and what `R12+0x290C`
   holds is **OPEN**.
4. **The list file's loader** — the file is identified by its name (in the exe at `0x1425DC952`)
   and by its content (the six lengths and the Beckham ids), but the code that reads it was not
   isolated this pass. The port reads it through the VFS winner, which is what PES does.
5. **Bolt's animation** — **there is none in the executable**. If PES gives him one it is data.
   Which data, is **OPEN**.

---

## §12 Method

- Bridge decompiles (`dec/FUN_*.c`) and disassembly (`dec/dis_*.txt`, `dec/dis_at_*.txt`) for
  every function named above.
- The six lists were read from the install with
  `build/apps/pesfmt_dump cat common/anime/playerID/bin/Derivationdatatable.bin --pes21-root <workdrive>/PES21`
  and decoded by script (`<workdrive>/gf-TeamDecode-scratch/deriv.bin`).
- Every motion name came from
  `build/apps/pesfmt_dump motions N --pes21-root <workdrive>/PES21`.
- Every constant was read by script from `pes_full.bin`.
- The id scan (`§9`) scanned the whole executable section for `cmp reg, imm32` with
  `imm32` in 100..400000 and checked the preceding call.
- **Self-check** (`<workdrive>/gf-TeamDecode-scratch/check_specialplayers.py`):
  ```
  -> 106 list entries read (106 total)
  -> 106 list rows checked
  -> 106 sub ids checked
  -> the six list headings checked
  -> the four packing constants checked
  -> 47 id constants checked
  -> 11 float constants checked
  -> 106 motion-name pairs checked against pesfmt_dump
  -> 15 textual claims checked
  -> 7 id mentions checked against the image's compares
  -> 8 further claims checked
  cited code VAs not inside a disassembled instruction: 0
  TOTAL mismatches: 0
  ```
  The checker verifies every one of the 106 list rows (the player id, both motion numbers and
  the sub id) against the file, re-reads every motion **name** back from `pesfmt_dump`, verifies
  the 47 id constants and the 11 float constants against the image bytes, and checks that every
  id the report names is one the image actually compares.
  **Mutation-tested — all fifteen fire:** a list row's player id; a list row's motion-in; a list
  row's motion-out; a list row's sub id; a motion name; a list heading; the flag value; the style
  bit; CR's action kind; the "2344 is not an id compare" claim; Bolt's uniqueness; the tail's
  compare VA; the 112.5 constant; the Beckham id; and the list file's path.
- **The one thing this pass could not do**: isolate the code that *reads* the list file. The
  file is identified by the name in the exe (`0x1425DC952`) and by its content (the six lengths
  and the Beckham ids, exactly). The port reads it through the VFS winner, which is what PES
  does, so the port does not need the loader — but the loader's VA is **OPEN**.
- No GF code changed, no GF build/test run, no PES write/pin/input.
