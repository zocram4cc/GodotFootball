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
