# PES21 hard-coded player IDs (Main, 21-10-26)

Found by scanning the image for the player IDs as 32-bit immediates (capstone decode of each hit), then decompiling the
sites. IDs from the owner; names are not asserted here.

## 1. A curled long-kick swap for five players (FUN_14071E570)
`FUN_14071E570(player, row, column, flag)` picks a kick clip from the table (FUN_1406ABB30). If the player id
(FUN_1406EA7B0) is one of **815, 118718, 123447, 134448, 134563** (compares at 0x14071E5C3..0x14071E5FF), `flag == 0`,
the picked clip is 0x93D/0x93E (2365 `kick_long_3_0_inside_y0_045_curve`, 2366 `…_curve_beckham`) or 0x948 (2376
`…_curve_max`), and the anime interface's vt+0x28 returns 8, the clip becomes **0x93F = 2367
`kick_long_3_0_inside_y0_045_curve_beckham_2002`**. Otherwise FUN_1406B3560 (below) may substitute a per-player clip;
then 0xA5E → 0xA7B and 0xABE → 0xADC.

## 2. A per-player motion substitution table (FUN_1406B3560)
`FUN_1406B3560(table, playerId, motion, kind)` walks one of six lists (kind 0..5: 8, 0x23, 0x17, 0xF, 0x10, 9 entries)
and returns a player-specific replacement motion. IDs 134448 and 134563 also appear as compares inside it
(0x1406B36C1..0x1406B371D). This is the general signature-animation mechanism; its list contents are open.

## 3. Player 33185: six gameplay sites
|site|function area|what it does (decompile)|
|---|---|---|
|0x14058629F|team/AI pass gate|for him (and not bVar2) the gate returns false while fps·3.0 < a distance term|
|0x14074D430|kick/action setup|sets flag 0x20 in the action word|
|0x1409DF937|positioning (reads the team point +0x2482C)|a different depth/position branch with 5.0 (0x14259222C)|
|0x1409E0633|positioning|a special flag, cleared when a distance < a scaled value|
|0x14210601A|COM brain|a branch on action 0x15 when he is the player (team designation FUN_140A62D90)|
|0x1421164C2|COM brain|the evaluator returns 0 unless he is the player and team byte +0xB6DE is set|

Open: who each ID is in the stock database; the FUN_1406B3560 lists; each 33185 branch's full rule.

## 4. Systematic scan: every constant compare after a player-ID getter
Every `cmp reg, imm32` (100..400000) within the instructions following a call to FUN_1406EA7B0 (player id) or
FUN_140A62D90 (team slot → id). 2376 is a clip compare (FUN_14071E570), not an id. Names are not asserted; the
owner can map ids to players. ID 7511 has no compare anywhere.
|id|sites|what the first decompiled site shows|
|---|---|---|
|972|0x14054C57A, 0x1405573A7, 0x14056321E, 0x1405948E7, 0x1405A5B30, 0x1405DC5CA (A62D90)|team-AI band; not decoded|
|1624|0x1406419EB, 0x140641BE2, 0x14080EA54, 0x14081CECB, 0x1409945C5, 0x14099519A (A62D90)|not decoded|
|1630 / 102618|0x1407DAE39/0x1407DB429 and 0x1407DAE2A/0x1407DB41A|either id (or a flag) admits a branch when param_5 is 13/14 or 3/4 (action kinds)|
|1631 / 4522|0x140727040 / 0x140727031, 0x1407D98A6, 0x1407D98E5, 0x140823A65|either id (or style 0x1C via FUN_1406EAD40) sets FUN_1406FA9F0/FAAA0(player, 1)|
|2344|0x14085EE4E (A62D90)|not decoded|
|34873|0x14084D93E|not decoded|
|34908 / 40352|0x1407DDF7C / 0x1407DDF69, 0x1407D9D03, 0x1407D9D69, 0x14047B172|40352 forces param_3 = 0x93 / 0x94 (an action/clip kind) at 0x1407D9D03..|
|115894|0x1406CC720 (A62D90)|in MOVESTEP's band: jumps to LAB_1406CCA88 for him|
|33185 and the five kick ids|§1, §3||

## 5. Names (looked up 21-10-26: pesdb.net/pes2021?id=N, pesmaster.com player pages)
|id|player|site behaviour (§1-§4)|
|---|---|---|
|815, 118718, 134563|D. Beckham (three cards; pesdb)|the beckham_2002 curled long kick (§1)|
|123447, 134448|not in either 2021 database [INF: further Beckham cards, by their place in the same compare chain]|same; 134448 also in FUN_1406B3560|
|33185|M. Neuer (pesdb)|six branches incl. two keeper-positioning sites reading the team point (§3): the sweeper keeper|
|972|F. Inzaghi (pesdb)|six team-AI sites (A62D90), not decoded|
|1624|R. Carlos (pesdb)|six sites, not decoded|
|1630|Romário (pesdb)|with 102618, an action-kind branch|
|102618|H. Mastour (pesmaster)|same branch as Romário|
|1631|Ronaldinho (pesmaster)|with 4522: the two per-player flags|
|4522|Cristiano Ronaldo (pesdb search, pesmaster)|same flags as Ronaldinho, plus three more sites|
|34908|Marcelo (pesmaster)|with Neymar|
|40352|Neymar (pesmaster)|forces action kind 0x93/0x94|
|34873|Diego Souza (pesmaster, PES 2020)|one site|
|115894|U. Bolt (pesmaster, PES 2018)|a jump inside MOVESTEP's band (0x1406CC720): a locomotion special case|
|2344|not found in either database|one site|

## 6. Bolt (115894), read in full (Main, 21-10-26)
- The id occurs exactly once in the whole image: `cmp eax, 0x1C4B6` at 0x1406CC720, inside FUN_1406CBF70 (a MOVESTEP helper: it writes the rate word req[0x4A] and, in kind 3, issues a clip from DAT_1425DDCF0 at [style req[0x50]·0x10 + band]). No data table holds the id.
- Normal path (FUN_1406CBF70 lines ~175-245): team/designation checks (FUN_140A62690, FUN_140A627E0, FUN_140A62960, the chaser state 0x32, T+0x255), ending in FUN_1406E6660(player, 0x15) (a byte of the player's stat record).
- Bolt's path: when the side's +0xB6E0 bytes are clear and he is slot < 0x16, the id match jumps straight to LAB_1406CCA88: the rate from FUN_1406E66D0, a ×DAT_1425AB124 scale in one carrier-facing case, req[0x4A] = rate, the band 4/8/12 from the rate thresholds, then the style×band clip. So the id check skips the designation block and goes straight to the run rate and the clip pick.
- No motion name in the exe's motion table contains "bolt", "usain" or "sprinter" (`pesfmt_dump motions`). Any Bolt-specific animation is therefore not selected by id here; it would come from his style word req[0x50] (the locomotion style, P+0x27B1) or from data. Open: the claim from another RE source that the id selects a special Bolt animation: needs the motion number and the site.
