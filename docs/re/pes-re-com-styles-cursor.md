# PES 2021: COM playing styles vs the user cursor (external agent Q&A, 20-10-26)

Source: an outside RE agent, archived in substance with names removed. Main's audit is marked; the rest is the agent's claim.

> **Main audit 20-10-26:** `0x1406b76c7` in the movement planner is `call 0x140479470`, and 0x140479470 begins `cmp edx,0x15` (a control-slot index check), which fits "the planner checks the control slot". The other VAs land on plausible instructions (`0x14210e4a7` is a call; the SB reads at `0x14211A98A`/`0x142130A67` are `mov ecx,[reg+0x4c]`). The semantics are not re-derived.

## COM styles run on players the cursor is NOT on
- Every player has an AI brain (image admissions such as Breakthrough, the dribble planner, trick selection); every COM-style effect lives inside it.
- A control-slot table holds (player id, control flag) per slot; the cursor's slot is flagged user-held.
- The brain's per-frame driver RESETS the brain (not just pauses it) for a user-held player: selected image cleared, intention reset, cached answers wiped. sub_142110C90 resets via sub_14210E5F0 when predicate sub_14210E200 fails, i.e. when "my team has a cursor slot AND my slot flag is clear" (0x14210e4a7). CheckPlayerSlotActive 0x140479470: flag clear == cursor on the player.
- The AI movement planner also skips planning for cursor-held players (MoveFamily_PlanLocomotion, 0x1406b76c7); their movement is stick input plus low-level physics, which never reads styles.
- So a COM style is live exactly while its player is AI-driven. The CPU team has no cursor: its styles are live all match. A "live" style still sits behind the brain's ball-relevance gates (position compatibility, trap state, ball zone).

## COM styles are on-ball only
- All seven COM styles are on-ball decision modifiers: image admission scores (breakthrough, cross, long shot, middle shot, curve shot), feint/trick selection, dribble-planner access. Example, Speeding Bullet: breakthrough base 20 instead of the double-touch 70, plus a side door into the marker-beating planner (reads at 0x14211A98A / 0x142130A67).
- Off-ball behaviour (positioning, runs) is a different system that runs for everyone regardless of the cursor, and reads the regular playing styles (21; Goal Poacher, Dummy Runner, Fox in the Box, ...) through CheckStyleAtPosition in the positioning layers.

## Port note (Main)
GF's PES path has no user-cursor exclusion yet. When human control is on the PES path, the brain reset and the planner skip for the held player belong with it.
