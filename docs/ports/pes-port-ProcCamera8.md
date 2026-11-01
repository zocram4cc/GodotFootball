# ProcCamera8: decode of the CAMERA_TARGET writer (registry slot = fdc Human slot + 1; scorer = slot 0, conceding keeper = slot 21), of the camera-list builder `0x141F2DD80` (the selection that was "selectByRate" is a set word mod set count plus one RNG pick per tens group), and the scorer jump traced to GF's sim restart schedule. NO GF code change this session: decode and tooling only (01-11-26)

Branch `CutsceneTiming` (worktree `gf-CutsceneTiming`). Labels: CONFIRMED = decompile (Ghidra bridge) or read from a full capture this session; INF = inference; OPEN = blocker named. Captures read: `live2/pid3741448/seq71718, 134037, 137125, 142542, 11590`, `live2/pid1043691/seq16574, 17246` (all proc_camera_141F7BFE0, full). Scratch (dumps, decompiles) is `gf-ProcCamera8-scratch/` (PES-derived, not committed). The ticket was cut short by the owner's handover: items 2 (feed), 3 (port), 4 (fix) and 5 (frames) are **not done** as code or frames; what they need is decoded below. Budget note: no GF build or movie was run this session.

## 0. Status against the ticket

| ticket item | result |
|---|---|
| 1. merge `origin/main`, push | DONE: `469be70c` (merge, docs only), pushed. |
| 2. CAMERA_TARGET writer decode, slot->actor map | writer DECODED (section 1); slot map DECODED for the slots a goal needs, except the team-mate/opponent permutation (OPEN 1). Feed in `match.gd` NOT written. |
| 3. `selectByRate` / which record is in force | the selection decoded and checked on a capture (section 2); NOT ported. |
| 4. step-2 scorer jump | root cause located by reading (section 3); NOT fixed, NOT confirmed by a run (OPEN 4). |
| 5. frames of each step | NOT done (no build, no movie). |
| 6. gate line | section 5. |

## 1. The writer of the 28 CAMERA_TARGET records (CONFIRMED)

**`0x141F2EBB0(player)` is the per-frame registry update of the FixDemo player** (the registry users of format `0x142608750` are `0x141F36450` (the ref initialiser, called with group 0 from `0x141F55110` and group 2 from `0x141F409E0`), `0x14089AEE0`, ...; the actual writer is this function, found through xrefs to the handle type `0x142608698`/the ref vtable `0x142608738`). What it does, in order:

1. Clears all 28 records of the table at `player+0xC88` (`valid = 0`, ground, the two joint points and facing zeroed): `0x141F2EBB0` loop over `0x40`-byte refs, `< 0x700` bytes.
2. Walks the player's **human map** (`std::map` at `player+0xB38`; node `+0x20` u32 = the fdc Human slot, node `+0x30` s16 = index into the actor array `player+0xC70`, node `+0x32` = active). For each active human it writes the record at **registry slot `index + 1`** (`iVar1 = (short)node[6] + 1`, `< 0x1c`): `valid = 1`, ground = actor `+8..+0x10`, facing = `0x141EA2F60(actor)`, joint point B (`record+0x1C`) = actor `+0x7C` (joint 0), joint point A (`record+0x10`) = actor `+0xF4` (joint 10).
3. Writes the same data into a second table (`player+0xC90`) at slot `role + 1`, where role = actor `+0x36D` for body kinds 0/3/7, `+0x36D + 0xB` for 1/4/8, `+0x36D + 0x16` for 2 (officials, `< 4`). The camera of a goal reads group 2 and found only the first table's slots valid in all seven captures; the second table (group 0 by construction) is INF the live-play targets.

Capture proof (`fixdemo_player_dump.py`): in seq71718 the camera's valid records are slots 1..5 and the human map has fdc slots 0..4 with `idx == slot`; the record of slot `s+1` holds the position of the actor of fdc slot `s` to three decimals (e.g. slot 1 = the actor at (45.408, -24.706), joint 10 at 1.616 m, joint 0 at 0.965 m, facing 334.867). So **a camera record's target byte `b` points at fdc Human slot `b`** (byte `+0xB1 = 0` is the scorer, as the proof of ProcCamera4 found). seq142542 (byte 22): valid slots 2, 13, 23 = fdc slots 1, 12, 22 (22 = an official, key 22, roster member 0). seq11590 (byte 16): fdc slots 13..16.

The 8-byte entries of the cast: `[id u32][kind][side][roster slot][flag]` (`0x141F56600`: id low byte = fdc Human slot for ids `0x13xx`, kind = side, side, roster slot, flag). The creation list at `mgr+0x78` is the plain XI fill (`0x141F561A0`: ids 0..10 side 0 roster slot i, ids 11..21 side 1), so the actor key is `side * 11 + roster slot`, member = roster slot (CONFIRMED on all 22 FixDemoHuman objects of seq71718: keys 0..21, kinds 0/1, member = key or key - 11). The slot list `mgr+0xA8` maps fdc slot -> (side, roster slot) per category; the goal builder is `0x141F5FC00` (category 3), and `0x141F5F200` turns it into a map fdc slot -> actor key (`kind < 2`: `slot < 0xB ? side*0xB + slot : slot + 0xD`; kind 2: `slot + 0x16`); `0x141F563E0` gives every actor not named an unused fdc slot from 0x1B up; the event overrides (`0x141F56A30` -> `0x141F56BB0/56C70/56D30/56DD0/56900`, swapping through `0x141F79F10`) put the flagged / event men into named fdc slots.

### What GF can name from its state (goal scene, scorer side S)

| fdc slot (= target byte) | registry slot | PES actor | GF source | verdict |
|---|---|---|---|---|
| 0 | 1 | the scorer: the event man (`ctx+0x5214` side, `ctx+0x5218` roster slot; in the captures `ctx+0x5218` = the actor's key) | `gateScorer_` (`cutscene_actor`) | CONFIRMED (4 captures: slot 1 = the actor whose member equals `ctx+0x5218`; in seq134037 member 9, in seq71718 member 10) |
| 21 | 22 | the conceding keeper: `0x1315 -> (side 1 - S, roster slot 0)` | the other team's keeper (`PesSlot` side `1 - S`, slot 0) | CONFIRMED: seq71718's cast list holds `0x1315: side1 slot0`; the builder `0x141F5FC00` case `0x15` writes `(S^1, 0)`. This is ProcCamera7's step-1 target byte 21 |
| 22, 23 | 23, 24 | referee, assistant (`0x16`, `0x17`: kind 2 roster 0 and 1) | GF referee (`PlayerOfficial`), position only | INF: seq142542 slot 23 is key 22 member 0 |
| 1..6, 9..11 | 2..7, 10..12 | scorer-side team-mates | see OPEN 1 | the builder's table and the captures **disagree** |
| 12..18 | 13..19 | opposing outfield men | see OPEN 1 | same |

The builder `0x141F5FC00` writes fdc 1..6 -> roster slots 9, 8, 7, 6, 5, 4 and 9, 10, 11 -> 2, 1, 0 (own side) and 12..18 -> 1..7 (other side). The captured cast lists are different: seq71718 holds fdc 1..4 -> roster slots 7, 8, 9, 1 and 13..16 -> (side 1) 1, 7, 3, 4; seq16574 holds fdc 1..4 -> 1, 7, 9, 8; seq17246 (side 1 opponents) holds 13..16 -> 8, 2, 9, 3. Same ctx roster in seq71718 and seq16574 (`ctx+0x5218` = 10 in both), different men: the permutation is drawn (OPEN 1), not the builder's fixed table. **Do not port the builder's table for the team-mate and opponent slots**: it would frame the wrong men.

## 2. The camera list: "selectByRate" is not a rate (CONFIRMED)

The list the cut player plays (`player+0x88`) is built at cut start by **`0x141F2DD80`** (offsets `+0x78`, `+0x7C`, `+0xA8`, `+0xB58`):

```
sets  = player+0x7C                      (consecutive hundreds groups of camera ids, from 0x141F3CFA0's tail)
h     = player+0x78 mod sets             (0x141F2DD80: (+0x78 % +0x7C) * 0xF0)
for t in 0..9:                           (the ten tens-vectors of set h: ids h*100 + t*10 + k)
    n = size(vector[h][t]);  if n > 0:
        i = 0x141F7A100(scene+0x178, n)  (one RNG draw: ((0x140A924E0(rng,0x2AC,2) + rng[0x20] + 0xAA) & 0x3FF) * n >> 10)
        r = vector[h][t][i]
        if list.size < 2 or r.start > list.back().start: list.push_back(r)   (equal start dropped after the second entry)
```
The camera ids are grouped by `key / 100` (set) and `(key % 100) / 10` (tens group = alternates); `0x141F3CFA0` fills the vectors from the record map at `player+0xB58`. The 23 bytes at record `+0xD8` ("SelectByRate" in the earlier docs) are read by the engine as style bytes; this decode found **no read of them in the selection**.

Capture proof: `seq137125` player has `+0x78 = 45`, `+0x7C = 2`; its camera map holds ids 0 and 100 (both mode 6, target bytes 9, 10); the built list holds the **id-100** record (style bytes differ from id 0's): `45 mod 2 = 1`. Other captures (`+0x78`, `+0x7C`, list size): seq16574 31/1/2, seq17246 66/1/1, seq142542 49/1/1, seq11590 52/1/1, seq134037 79/1/1, seq71718 71/1/2 (ids 0 and 10: two tens groups, start 0 and 300 both kept). Each layer is its own player with its own list (INF: the layer players; the camera controller's choice **between layers** is `0x141EB0570(owner+0x258, cam)`, not decoded: OPEN 2).

What replaces ProcCamera7's stand-in ("first record of the layers starts together wins"): per layer, build this list, then take the controller's pick. Stand-ins still needed: the set word `player+0x78` (its writer is not in the 335 functions of `0x141F3xxxx-0x141F4xxxx`; values 31..79 in the captures: OPEN 3) and the scene RNG at `scene+0x178` (its seeding is not read).

## 3. The scorer's position jump at step 2 (cause located by reading; NOT confirmed)

GF's scorer in `cutscene_actor` is the live sim player, steered by the sim (`SetGoalCelebrationSpot`: the scorer runs to PES's T). The sim runs the whole time: `GfMatch::step` calls `match_->Process()` every tick, while PES's DemoInfo runs the scene. GF's goal sequence (`goalsequence.cpp`: replay then `RestartPrepareAt_ms` / `RestartKickOffAt_ms`, scheduled by `referee.cpp:367` at the goal tick) is time-based and does not know the PES scene's length. `ReplayFiresAt_ms(goal, cutsceneEnd_ms)` takes the scene end as an argument but **nothing feeds it** (grep: no caller passes `cutsceneEnd_ms`), and `Referee::AlterSetPiecePrepareTime` (the push-back that moves prepare and kick-off together under `PesLoop()`) has one caller (the referee special hold). INF: the ~40 m move in 20 ticks at step 2 (frame 3206, 16 s after the scene start) is the sim's kickoff placement (`ResetSituation` / `PrepareSetPiece`) running under the scene; the fix is to hold the restart while `timeline_` is active (`AlterSetPiecePrepareTime(now + one tick)` each tick, or feed `cutsceneEnd_ms`), PES holding the match under the DemoInfo. Not confirmed: needs the per-tick scorer position plus the referee buffer's `prepareTime`/`startTime` against the jump tick in one movie run.

## 4. Open items (numbered, for the next session)

1. **Team-mate and opponent permutation** (fdc 1..6, 9..11, 12..18): the cast lists in the captures differ from `0x141F5FC00`'s table and between goals with the same roster. Find what permutes them: candidates are the info triples `(kind, fdc slot)` at the step struct `+0x1C` read by `0x141F56A30` (kinds 1,2 flag P+1, 3,4 flag P+0 (roster byte flags: slot 0 `P+0 = 1`, the striker `P+1 = 1` in the captured roster), 5/0x11 the id at `ctx+0x517C/0x5180`, 6 the coach record, 7 the event man), and a draw. Tool: `fixdemo_player_dump.py --cast` on more goals; needs the step structs (`FUN_141F61180(mgr, i)`, 0x1E0 bytes) from a capture's manager.
2. **Which layer's camera is in force** when several layers have records starting together (the controller `0x141EB0570`, `owner+0x258`): GF's `InForce` still takes the first layer.
3. **`player+0x78` (set word) writer and the RNG `scene+0x178` seed**; after them the per-layer list builder can be ported (the rule is section 2, with the unit test below).
4. **Jump**: one movie run with the scorer's position, `refereeBuffer.prepareTime/startTime` and `actualTime` logged per tick; then hold the restart while the PES timeline runs.
5. **Frames** of each step (steps 0, 1, 2) and a PES goal burst: not taken. The rig is up (`live2/pid1043691`); a burst needs the owner's OBS rig.
6. **Feed**: once 1 is closed, `GfMatch::cutscene_actor` becomes the list of {fdc slot, player index, ground, facing}; `match.gd::_advance_cut_camera` calls `set_target(slot + 1, ...)` for each. Until then slots 0 (scorer) and 21 (conceding keeper = `PesSlot` `(S^1)*11 + 0`) are the only ones confirmed. Slot 21 is cheap and would turn ProcCamera7's step 1 (target byte 21, now an empty record aiming at the pitch centre) into a keeper shot.
7. Referee/assistant slots 22, 23 need the GF referee's skeleton or a ground position only (the engine reads the two joint points too).

## 5. Tests and gate

No code under `src/`, `godot/` or `tests/` changed, so no engine test applies. Added `tools/dev/cutscene/fixdemo_player_dump.py` (read-only; run on seq137125, prints the two valid records, then the player blocks of this report). `GATE fast (interim): not run, no engine change; Main gates. gen_translated.py --check not re-run (no change to the layer).`

Files: this report, `tools/dev/cutscene/fixdemo_player_dump.py`.
