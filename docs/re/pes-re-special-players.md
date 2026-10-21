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
