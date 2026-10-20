# PES 2021: the action-state +0x3C motion substate (external agent Q&A, 19-10-26)

Source: an outside RE agent (owner's copy-paste), archived verbatim in substance. Main-audited points are marked.

> **Main audit 19-10-26:** the Kick-class store `0x140727E5F` `mov [rdi+0x3c],bl` matches; the record table `0x14344E380` (16-byte records) byte+12 takes only values 0..13 over its 151 records (script read). Not audited: the Feint-class writer path, the group-id lists, the name-table labels.

## What +0x3C is
- A MOTION SUBSTATE byte in the action state, not the action kind (the kind is at +1).
- The player entity carries a motion-group id at +0x2752 (word; -1 = none; valid 1..150).
- When a kick or feint motion executes, the engine stamps +0x3C = record[player+0x2752].byte12 from the static 151-entry table at 0x14344E380.
- Writers (a sweep of all 94 action-class vtables found exactly two): the Kick class sub_140727D90 at 0x140727E5F, and the Feint class sub_1407E11B0 via sub_14071E4B0.

## Values (group ids)
| substate | groups | name-table labels (0x1434A5470; build-time linkage, INF) |
|---|---|---|
| 7 | 121, 127, 128, 137 | FEINT_KIND_ONETIME, KICK_NUTMEG, ELASTICOTURN, NO_TOUCH_AUTO (one-touch strikes, elastico-family touches) |
| 10 | 82, 130, 131, 134, 148 | PARRY_SIDE, PARRY_SIDETURN, SLIDE_PARRY, SCISSORS_OUT_L; 82 = fatigue/injury in-play motion |
| 11 | 116, 117 | KICK_OUT, KICK_OUT_SHOOT (never observed live) |
| 13 | - | default / no motion |

States 14 and 21 do not exist (domain 0..13). The morale loop's comparisons against 14 (spiritBoost) and 21 (teamMorale) are dead in PES. Only 7 and 10 are live; a 90-min log saw 7 twice and 10 six times.

## Consequence (the morale design as it actually runs)
Teammates one-touching or doing skill touches (7) and teammates mid dribble-feint (10) hold the team's fatigue cliff at 70% instead of 60%. GF does not port this rule (no morale/fatigue-cliff code found): a port item for the stamina layer.
