# NetDodge: post gate (pushed), chooser decode (no port), inventory facts

Final state. Worktree `the worktree`, scratch `the scratch dir` (kept).

## 0. State

- Pushed: `refs/heads/NetDodge` = `a254859` "pesdodge: the post dodge gate reads the dodger, as FUN_140a084d0 does", on origin/main `1ff9c3b`. The push used `--force-with-lease` on the old `NetDodge` ref `6f72b26`; that commit is already in main as `00b3875`, with the same content for the files it touched.
- Gate: one full `ctest` on `a254859`, `nice -n 19 ctest -j3 --timeout 2400`, `TMPDIR` in scratch: 709 of 709 passed, plus the 2 standing install skips (`VfsInstall.NestedOriginalsSurviveWithoutDownloads`, `EditSaveInstall.DecryptsAndMatchesTheRunningGame`). The three Godot tests passed (`godot_character_test` 1.56 s, `godot_player_test` 3.71 s, `godot_match_scene_test` 18.24 s).
- Piece 2 (chooser team/engaging gate): not ported. Owner pause; the findings go into the build order (section 4.4 and 4.5).

## 1. What the commit does

- `PostDodgeGate` (`src/sim/onthepitch/pesdodge.cpp`) reads the dodger's record against the goal marks. Conditions in FUN_140a084d0's order:
  - keeper and newest-event player are out;
  - edge `|x| >= F[0] - 1.0` (DAT_14256ae1c = 1.0);
  - z band `| |z| - F[2] | <= 1.0`;
  - anime slot (stand-in);
  - speed above `100 / (gs*72)` per tick, i.e. 100/72 m/s;
  - residual to the post folded to (-180, 180] and `<= 15 deg` (DAT_14259be0c), or `|x - post.x| <= 0.5` or `|z - post.z| <= 0.5` (DAT_142565600);
  - distance to the post `<= 5.0 m` (DAT_14259222c);
  - designated-man clause: if he is the designated man and `|ball.x| < F[0]`, a ball within 2.0 m of the post (DAT_142565604) and nearer him than the post refuses him.
- The post is at the goal line of his own end (sign of his x) and on his own side (sign of his z). The 6f72b26 port used the team's end and the ball's side; that was wrong.
- Removed: the 12 m window on the team's own goal line, and the 200 ms ball prediction as a gate.
- Kept unchanged: `PostAnglesFor` and `PostSelector`.
- Stand-ins, each marked `ponytail:`: anime start test `FUN_1406b4c10(ap,0x15)` open (as in the net gate); newest-event player = GF last touch (FUN_140a60300 tuple); designated man = GF designated possession player (`MA+0x2470c` not carried); movement state `+0x5b0` not carried.
- Shared with the net gate: edge 1.0, residual 15, speed 100/72. Post-only constants read from the install: 5.0, 0.5, 2.0.

## 2. Tests and measurements

Tests: 44/44 dodge tests (9 `PesDodgePostGate` tests and the install relation `TheGoalGateConstantsAreTheExesOwn`, which needs `PES21_ROOT`).

Mutations, each run after its own rebuild, each killed by a named test:
- M0 gate stubbed to `false` (RED): all 9 post tests fail.
- M1 edge margin 2.0 m: `HeHasToStandWithinTheEdgeMarginOfHisGoalLine`.
- M2 z inner edge dropped: `TheZBandHasItsInnerEdgeToo`.
- M3 x half-metre admission dropped: `HisZMustSit...`, `TheZBand...`, `HeadingAtThePost...`.
- M4 distance cap 6 m: `TheFiveMetreCapBoundsHim`.
- M5 ball clause inverted: `TheDesignatedManIsKeptOff...`.
- M6 keeper exclusion dropped: `TheKeeperAndTheNewestToucher...`.
- M7 x sign fixed to +1: `HeHasToStandWithin...` (negative end).
- Source restored, md5 `5d30ffea98b285f73faf11ab4dce76d3`, 44/44 pass again.

Smoke, teams 1 v 2, seed 7, 20 min, the handover's arguments:

| run | `[dodge]` ball | net | post | human | played dodge | passes | forced | red | determinism |
|---|---|---|---|---|---|---|---|---|---|
| origin/main `1ff9c3b` | 4807-5268 | 23-38 | 942-1225 | 1679-1657 | 1094-1155 | 58-52 | 11487 | 0-0 | 9191772f1f1ab525 |
| `a254859` (this piece) | 5733-8807 | 11-3 | 3-6 | 1988-1815 | 733-835 | 49-56 | 13129 | 1-0 | 45b5c9894844c576 |

Same-base A/B on the `6f72b26` tree, with and without the post commit: post 2121-784 -> 3-2; played dodge 1198-1109 -> 835-888; forced 10756 -> 8550; determinism 19a8569ee718dd6b -> 6acf7c6f1ec4d78a.

Reading: the 12 m window admitted most post dodges; PES's gate admits a handful per match. The other rows moved because the sim is chaotic: removing about a thousand post dodges changes every later decision. The forced-stall rise and the red card are trajectory effects, not gate outputs. They are reported as measured.

## 3. Gate history

- Run 1 on `ee44049` (same tree): 692 of 695 passed. Three Godot tests hit the 120 s limit; their output stopped after the engine banner. Rerunning those three together at `-j3`: 3/3 passed (1.5 s, 3.5 s, 17.6 s). Cause not pinned; likely the fleet load or concurrent Godot imports in a fresh worktree. Not reproduced in run 2.
- Run 2 on `a254859`: 709/709 (section 0). This is the gate for the push.
- Disclosure: before the owner's rule arrived I started a Godot-only `ctest` (`-R godot_(character|player|match_scene)_test`, `-j3`, passed 3/3 in 17.6 s). It finished before I was asked to stop; nothing of mine ran afterwards until the approved gate.
- The gate used `TMPDIR=the scratch dir/tmp`.

## 4. Decode facts (raw and decompile; VAs given)

### 4.1 Call chain
- `FUN_140925470` (per-player loop) calls `FUN_140926b00(AI, MI, team)`: a switch on `MI+0x11b0`; open play `0x14..0x16` returns 1 when `FUN_1409270f0` is 0. Then `FUN_140928fc0` (the tick), call at `0x1409294e4`.
- `FUN_1409257b0` runs only if `AI+0xc76e != 0`. The one literal read is at `0x140925819` (movzx). Its call to `FUN_1409f4db0` is at `0x1409265d6`.
- `FUN_1409f4db0` calls the chooser `FUN_140a06dd0` at `0x1409f5177`. That is its only caller.
- Special path of the loop: if `G+0x1a8 +0x8d` is set, the tick runs only for players whose int at `G+0x1a8 +0x24 +4*id` (via `FUN_140a60550`) is 2 or more.

### 4.2 Chooser gates
- `MI+0x1775` equal to `0x0b` or `0x17` means no reaction.
- Mode `m = FUN_140a06fd0(pc)`:
  - `FUN_141e534f0(id) == 2` (ids 22-24): none.
  - `id > 21` or `MI+0x1748[id] == 0`: 3, dodges only.
  - Keeper slot `T+0x4c00[id%11] == 0`, and `T+0x255 == 0`, and (`MA+0x251fc[team] != 0` or newest event type 6): 1. The mode-1 list is `{0x140933ad0 contact, 0x140a07dd0 net, 0x1409f8220 receive, stub 0x141863350}`, walked only when the newest event type is not 6.
  - Otherwise 0: the full 17-entry list, which includes all four dodges.
- Table entries are `{fn, flagA @+8, flagB @+9}`, 16-byte stride. m0 at `0x14263ec60` (17), m1 at `0x14263ed70` (4), m3 at `0x14263edb0` (4).
  - flagB on entries 1 (tackle), 2 (slide), 3 (receive), 8 (block); flagA on entry 2; dodges 4-7 have neither.
- Winner: `cmd+0x880 = 1`, `cmd+0x87c = 0x18`; the block exception is `cmd+0x878 == 0xe && cmd+0xfd5 != 0`.

### 4.3 Team and engaging terms that do exist
- Net gate `FUN_140a07e40`: no team term. It reads only the dodger, the marks and the anime slot. Its dispatcher `FUN_140a07dd0` checks `FUN_140ca1a50(0x23)`, which is `mov al,1`.
- Post gate `FUN_140a084d0`: keeper exclusion `FUN_140a62690(T, team) == id`; newest-event tuple test `FUN_140a60300(FUN_140a3e7d0(T+0xe0,·,0), FUN_140a62fe0(T,id))`; designated clause `T+0x78 == id` or `MA+0x2470c[team] == id`.
- Ball dodge entry `FUN_140a070e0`: reads the distance rank `MA+0x247be[id]` and the distance table `MA+0x24710` (team block 0 or 11) against the ball's predicted distance. This is the engaging-man notion, but for the ball dodge only.
- Consequence: outfield attackers reach the net gate in PES's code too. What differs from GF is the busy rule (4.4) and the keeper mode 1 (4.2). GF has neither.

### 4.4 Post-chooser busy rule (`FUN_1409f4db0`, after the chooser returns 1)
- The chooser's command is copied to `this+0x3c70`; the movement goes to `this+0x2290`.
- If `this+0x44e8` has bit 12 set (stagger/fall), the reaction (`+0x3c70`) is used.
- Else, if `this+0x2b08` has bit 16 (kick) or bit 4 (action 18 in the dumped table) set, the movement is used and the reaction is dropped.
- Else the reaction is used. Extra branch: `FUN_140a6e5e0(this+0x44e8)` (bit 5, trap) with `FUN_140a6e340(this+0x14)` (kick) calls `FUN_1406e1d60`.
- GF has no equivalent. A GF stand-in exists (`Match::PesActionOf`, KICK covering 8-10), but the identities of `+0x44e8` and `+0x2b08` are not pinned, so nothing was ported.

### 4.5 Keeper mode 1 needs two unknowns
- `MA+0x251fc[team]`: writer not decoded.
- Newest event type 6: the event kind is not mapped to GF.

### 4.6 Post selector (for the next item)
- `FUN_14073c9c0` requires `*(pc+8) >= 45` (DAT_14259be30). Its anchor argument is the ball now (`FUN_140a57800` = `FUN_140a6cf70(ring, 0)`).
- `FUN_1407368d0`: vector A is `pc+8/+0xc/+0x10`; vector B is `pc+0x14/+0x18/+0x1c` (`FUN_141eb50b0` = A minus B). Bearing = `FUN_14046f230(A-B)`, raw 0..360, minus refAngle `pc+0x20`, folded. Near branch below 45 deg; far branch for 60 < |residual| < 120 (DAT_14259a764, DAT_14256ae30); outside that window kind 3 gets no row.
- The approach is `FUN_140474c40(A -> ball)`, then `FUN_1406e4230` (folded minus refAngle). Here the ball is read at time 0.
- GF's `PostAnglesFor` uses an own-post anchor, with the sign taken from the ball's side, and approaches from the 200 ms prediction. B's sign is not pinned; the anchor and approach differ from PES's selector inputs.

## 5. Inventory facts (VA citations; GF status in brackets)

- `G+0x2aa8`: FieldInfo float pointer, `F[k] = [ptr+4k]`; `F[0]` at `[ptr]`, `F[2]` at `[ptr+8]`, `F[14]` at `[ptr+0x38]`. Gates dereference it each tick; the FieldInfo doc says null (already noted by Main). [GF: static constants.]
- `G+0x40`: ball context; ring entry 0 is now (`FUN_140a6cf70`). [GF: `_mentalImage->GetBallPrediction(0)`.]
- `G+0x1a8`: situation object; byte `+0x8d` selects the special path; int table `+0x24 + 4*id`. [GF: none.]
- `MI = G+0x270`: `+0x116c` match state (1 in play, 2 stopped, 3 set piece); `+0x1775` FSM state (0x0b and 0x17 block the chooser, 0x18 changes `bVar12`); `+0x11b0` (open play 0x14-0x16); `+0x1748[id]` on-pitch byte; `+0x1198` frame. [GF: IsInPlay, IsInSetPiece, partial.]
- Player record `pl(i) = FUN_140a48a10(G,i)`: `+0x55c/+0x560/+0x564` position (x, height, z); `+0x574/+0x578/+0x57c` velocity in m per tick; `+0x554` facing; `+0x5b0` movement state (1 gives zero velocity; 5 gives zero below t^2 with t = 0.694444/gs); `+0x5d0` AI action. [GF: GetPosition, GetMovement in m/s.]
- Team `T = FUN_140a48a30(G,team)`: `+0x78` designated player (post clause); `+0xe0` pointer to the event context; `+0x18` pointer to MA; `+0x255` flag (meaning unresolved, section 6); `+0x4c00[slot]` position category (0 used as keeper); `+0x6a4` roster-slot array (duel doc). [GF: designated possession player approximates `+0x78`; roster slot stand-in 0.]
- MA (team `+0x18`): `+0x247be[id]` ball-distance rank; `+0x24710[rank + team*11]*8` distance table; `+0x2470c + team*0x58` designated index; `+0x251fc[team]` flag (writer not decoded). [GF: none for `+0x2470c` or `+0x251fc`.]
- Event context (`*(T+0xe0)`): newest event at `+0x180698` with type at `+0x1c`; ring A entries at `+0x000..0x1900` (count `+0x1900`, head `+0x1904`, stride 800); ring B at `+0x1908` (count `+0x3208`, head `+0x320c`), with an 8-byte tuple at entry `+0x1c`. [GF: `GetLastTouchPlayer`, approximate.]
- AI object: `+0xc76e` byte gates the tick (4.1); `+0xc76d` and `+0x7c8/+0x7c9/+0x7ca/+0x7cc` are flags in the loop. Think context `pc`: `+4` player id, `+8/+0xc/+0x10` vector A, `+0x14..+0x1c` vector B, `+0x20` refAngle, `+0x38` team, `+0x40` anime `ap`.
- Command `cmd`: `+0x874` AI action, `+0x878` anime action, `+0x87c` priority, `+0x880` reaction flag, `+0x894` target point, `+0x1094` kind, `+0xfd5` block flag.
- Action class table `0x1426484f0` (94 u32, indexed by action): bit 4 = action 18; bit 5 = trap (7); bit 6 = tackle (13); bit 7 = slide (11, 12); bit 12 = stagger/fall (16, 17); bit 15 = jostle (0x48); bit 16 = kick (8, 9, 10). [GF: `DodgeConstants::roleBits` read from the install; `PesActionOf` is coarse.]

## 6. Open conflicts and unknowns

- `T+0x255`: the counter-trigger doc (OBS) says "in possession", and its writer is only called with `T+0x255 != 0`. The defensive-shape and constant-team docs (INF) say "CPU team". The mode-1 keeper test depends on which holds. Literal disp32 search: 366 hits; four plain register stores (`88 /r`) at `0x1404167c2`, `0x140a428d7`, `0x1415d9840`, `0x14215d397`, not yet classified.
- `AI+0xc76e`: one literal encoding in the code section (the read at `0x140925819`). The writer is not literal, so it is written through a base register or copied from a snapshot.
- `MI+0x1748` (on-pitch): 26 literal hits, 10 store-like, e.g. `0x1406d9379`, `0x141e5eed3`, `0x141e5fd7a`, `0x141e60afc`, `0x1422bd5c3`, `0x1422bd5d1`, `0x1422bd606`, `0x1422bdb74`, `0x1422be6b7`, `0x1422bec56`. Not classified.
- `MI+0x116c`: 1130 literal hits, one store-like (`0x1411821c7`, `C6 /0`).
- `+0x44e8` and `+0x2b08` identities (4.4): inferred from the table bits, not pinned by a writer.
- Decompile sources for the old scratch directories (`gf-DodgePort-scratch`, `gf-PostDodge-scratch`, `gf-DodgeGates-baseline.txt`) were cleaned up. Main reports the text is archived at `the RE evidence archive /evidence/agents/<dir name>/` and `evidence/tmp-root*/`; grep there before re-decompiling. My own scratch (`gf-NetDodge-scratch`) is kept.

## 7. Next (not started)

1. Selector: pin B's sign and the approach time (PES reads the ball at time 0), then port the anchor per FUN_1407368d0. Its own gate, with a clean ctest, as before.
2. Piece 2, when the build order reaches it: port the post-chooser busy rule with `PesActionOf`, once `+0x44e8` and `+0x2b08` are pinned. Port the keeper mode 1 once `MA+0x251fc` and the event-type-6 mapping are pinned. Both are in the inventory and build order, not a new port now (Main's decision).
3. Pin the open fields in section 6 with the live readers (LiveTeamPlayer-2, LiveMatchBall-2), not by more static scans.
