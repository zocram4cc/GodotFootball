# CutsceneAnchor7: pos (0,0) and T.x are downstream of one wrong value, the else arm's scene id (A0+8 = 1, PES 0x300D9). Not green; the feed is two GF-native states, not the scorer or ball position.

Branch `CutsceneAnchor`, head unchanged at `f6ffbad5` (no commit this round: the only code change was a temporary probe, reverted; `git diff` is empty and no untracked files under `src`, `tools`, `docs`). Labels as before: CONFIRMED = run or read; INF = inference; OPEN = named blocker.

## 0. Outcome

- Wide test: still RED (9 EXPECTs, DC 5/100/161: T.x, pos.x, pos.z). Not green.
- Root cause found for pos and most of T (CONFIRMED by probe and by PES's seq204 memory). It is not the scorer's position and not a tick sample: the anchor calc gets the wrong scene id.

## 1. pos: traced back to its read (CONFIRMED)

- `0x140474F00` (the anchor calc) gets the scorer's man record from GF's arena correctly: a probe just before the call printed man pos = (-41.5502, 0.0000, 29.5574), the test's input. So the scorer position is not missing.
- The calc reads the scene row by A0+8 (the scene id). The probe printed A0+8 = **0x00000001** (PES: 0x000300D9). Index (0x1 - 1) is not a goal row of the test's table, so the row's modes/bounds are absent and the calc writes (0, 0).
- The install's row for 0x300D9 (decoded from the test's hex, the same bytes PES held): head modes (1, 0, 1, 3, 0); floats f4 = 34.0, f5 = 34.0, f6 = -34.0, f7 = -34.0 (the pitch box), f10 = f11 = 180.0. The expected pos z = 34.0 is the box edge; pos x = -41.5502 is the scorer's x.

## 2. Why A0+8 = 1 (CONFIRMED by reading the translated code)

The else arm's scene is set at `0x140471A80 → 0x1404B6BD0`: for BC < 6 (here BC = 2) it calls `0x1404B6400` and stores its eax into A0+8 (`s.wr(gpr7+8, gpr6)`).

`0x1404B6400` reads two GF-native states:
1. `T = [G + 0x2C0]`, the time-scale object (GF's `timeScale`, ponytail in `fixdemo_select.cpp`). It reads the key `u32 [T + 0x1C]`. PES, at the anchor (seq204), holds **T+0x1C = 0x71**. GF's object is zeroed, so the key is 0.
2. `0x141E5D4C0` reads the singleton `[0x1437F4808]`. If null it returns 0, and `0x1404B6400` falls back to its other path, which yields 1. PES holds `[0x1437F4808] = 0x346EC40`: a vector of **234** cut records (`0x141E5DE80` returns the count 234). Entry 0x71 is an object at 0x2D5BEFA0 whose +8 is a node of vtable 0x142B54228 (the cut's data: floats 16, ±80, 1). `0x141E5CA20` indexes the vector by the key and returns that node.

So GF lacks two pieces of game state the else arm reads: the cut-record singleton (234 nodes) and the key in T+0x1C. This is the same class as the engine singleton and the find-or-create creators in CutsceneAnchor6, but the table is game data (built from the FixDemo tables at boot in PES), not one object.

## 3. T.x (CONFIRMED values; cause INF)

- PES at the anchor (seq204, `A0 = rcx = 0x31C53DB8`): A0+8 = 0x300D9, A0+0xBC = 2, **A0+0x30 = (-41.621147, 0, 35.56)**. That equals the test's expected T, so the expected T is PES's own value.
- GF: T = (-41.5502, 0, 35.56). T.z matches. T.x equals the scorer's current x (-41.5502), so GF's x comes from the scorer's position rather than from the cut-driven computation (the row with modes is absent, so the else arm's fallback reads the man pos).
- INF (Main's hypothesis): PES's T.x is the scorer's x at the event tick (the scorer moved from x -41.62 to -41.55 between the event and the anchor, and from z 35.56 to 29.56: the z of T is the event tick's z). The sample tick cannot be tested until the scene id is right, because the else arm's T writer reads the scene row. Not verified.

## 4. Next blocker (named, not built)

Feed on the GF side, in order:
1. The time-scale object's +0x1C key (PES 0x71 at the goal). Source: PES's match state (not decoded in this round). Candidate: the goal's cut-group index.
2. The cut-record singleton `[0x1437F4808]`: 234 entries, entry k at vector[k]; entry +8 = a node of PES class vtable 0x142B54228. The node's fields for k = 0x71 are read by `0x1404B6400` (`0x1404B9730`, `0x1404B9890`, `0x141F500D0`, `0x141F4FC30` not yet read). Building the vector from the install's FixDemo tables needs the node layout and the goal's cut-group index mapping; it is not a one-line feed.
3. After (1)-(2), re-run the probe: A0+8 should be 0x300D9, then T.x and pos should follow (or show the next gap).

Per Main's rule (fix at the feed, not a stand-in): I did not add a value to hit the test. A fixture of 0x300D9 at A0+8 would bypass the else arm, which is the thing under test.

## 5. Checks

- No code change landed this round. `gen --check` and `ledger --check` are unchanged from CutsceneAnchor6 (rc 0 then).
- Neighbour suites: unchanged from CutsceneAnchor6 (registry 7/7, fixdemo_select 6/6, camera_list 5/5, proc_camera 6/6, goal-anchor non-wide 22/22, all on f6ffbad5's layer).
- One mutation: not run (no green change to mutate).
- Scratch: `gf-CutsceneAnchor-scratch/f5/seq204/` holds the index and call record of the anchor capture (mem.bin removed); the probe was reverted.
