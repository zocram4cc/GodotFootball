# PES port: TrapContact3 (branch TrapContact, on merge 6f2f78d0): the touch that leaves the ball at rest must hand the man to PES's dribble

Status: DIAGNOSIS CONFIRMED on this tree's own diagnostic run; fix NOT wired (it is a new slice). Tree is clean at the merge tip (diag prints reverted, nothing pushed). Evidence logs: `gf-TrapContact-scratch/diag3.log` (TCSCAN per-contact-pending-tick scan print + TCTRACE per-transition man trace; the producing prints are reverted from source but the log stands).

## 1. The finding (CONFIRMED, seed-7 pes-loop on this tree)

The touch chain through the grant is faithful. What happens AFTER a granted touch on a resting ball is not.

- H4, t=43444: enters Trap (ac8 7, phase 2, motion 0xdb5, clip `trap_0_0_000_y0_in`, contact frame 33), 0.65 m from a ball at rest.
- H4, t=43944: `touch 1 kind 4 vec (-0.02,0.03,-0.01)`: the scan GRANTS, kind 4 = ball velocity at `+0x9C`, and PES's own vector is ~0.03 m/s. The trap stops the ball at the foot; the ball stays at 0.08 m/s. Same shape: H9 0.08 m, A11 0.14 m, H5 0.25 m, H8 0.26 m, A2 0.02 m, A7 0.30 m — all granted, all ~0 vectors.
- After the grant H4's state never changes again (no further trace lines: he stays a Trap man beside a resting ball; TCTRACE prints every transition). No dribble starts, no retainer forms, the stalemate breaker fires.
- H5's full arc: granted at t=66851 (0.25 m), trap clip ends ~frame 121, GF plays a one-tick turn clip then a NEW trap clip (mirror), re-traps the same resting ball; later he leaves Trap (ac8 2 = MOVE at t=70722), becomes PES holder (holder 1) 1.1 m from the ball doing jockey/near_run clips with the retainer flickering. The dribble never engages.

So: grants touch (contact-census tests pass), the touch vector is PES's own ~0 for a resting ball, and the defect is that GF never runs PES's post-touch chain: trap end handler `0x14079E5C0` → action switch to MOVE (2, sub 0x5E) → act4 start `0x1407A38E0` → dribble with the ball. In GF the man ends up in an idle/walk/trap clip and the ball lies there; the GF breaker (5 s under 0.075 m/s with no retainer) fires. This is TrapAdmit2's A3 case exactly (admitted every tick, in Trap at 0.86 m, never touched for 138 ticks — the re-trap loop, since the carrier is never converted into a dribbler and the reaction keeps re-admitting him).

## 2. What exists in the layer vs what is missing (all CONFIRMED this session)

- End handler `0x14079E5C0`: translated, proven 6/6 REAL — but all six take the FIRST early return (playing motion still in the anime table). Beyond-early-return paths unproven. It IS called from the tick (two sites in `0x140796CE0`, closure member).
- Action switch `0x1406EEBB0`: translated, proven 31/31 REAL, and the record explicitly covers TRAP→4/8/9 "exit of TRAP 0x140797C90, enter of the new action" — so switch→DRIBBLE→act4-start runs inside the proven closure. TRAP→MOVE edge is the trivial ret vtable `0x1425E13F0`.
- act4 start `0x1407A38E0`: translated, proven 2/2 (1 REAL `pid615595/seq7805` + 1 SYNTHETIC). Its body: if `FUN_1406EC5F0(P)` false → switch(P, 2); else switch(P, 4) + `0x1407B6D80`/`0x1407A4D30`/`0x1407B68B0` (all in the proven `closure_trapact4` set).
- Action-4 (DRIBBLE) vtable = `0x1425E2440` (image read): +0x20 tick = `0x1407B37D0`, +0x28 = `0x1407B3650`, +0x38 = `0x1407BD430`, +0x58 start = `0x1407A38E0` ✓, +0x68 = `0x1407BA840` (planner) ✓.
- MISSING: the dribble tick `0x1407B37D0` (180-line decompile: holder gate `0x1406EC5F0`, touch-issue `0x1406B4580` kinds 0x52/0x53, then a 17-way sub-state dispatch over `0x1407B68B0/0x1407ABBF0/0x1407ACD70/...`, tail `0x1407C5010` + motion-set + `0x1406E2C30`) — NOT translated, not in any closure. This is the function that carries the ball (via sub-state touch builders → kind-4 records → ball kick application `0x140870A40`, also untranslated).
- MISSING wiring: a per-frame dribble runner (like RunTrapTick) for men with P+0xAC8 == DRIBBLE, plus keeping GF's `WriteActorP` from clobbering a PES-switched DRIBBLE back to the GF clip's action (today: `trapActive` protects only Trap bytes; a PES switch to 4 would be overwritten by the next tick's clip-derived rewrite, and no dribble runner exists to own them — extend the `trapActive` ownership discipline to DRIBBLE).

## 3. Two companion gaps (NOT this slice; named owners)

- (a) Strike adjuster never arms: EVERY TCSCAN line shows `A state 0 mode 1` — the arming (`FUN_141EB39A0` in trap start `0x14079C2D0`) never fires in the match, so the v1 0.2–3.3 m displacement gap remains and my `RunTrapAdjuster` wire is dormant. RootMotion owns this (why no phase-0 arming in GF's window).
- (b) GF-clip men near resting balls never enter Trap (A5 0.6 m in `highpass/idle/090`, A9 0.7 m in walk decel, H7 in shot/walk, A4 walking 4.1→0.7 m then idling): they carry motion 0xFFFF so gate 4a cannot admit them. MoveStepCell/AnimeLayer own PES motions for GF-clip men. Note A4's arc proves GF locomotion works — only the engagement is missing.
- (c) Close-range refusals H10 0.05 m ×2 / H2 0.27 m ×2 (resting ball): cannot judge from the trace — the reach test uses the motion's HIT-bone prediction, my print shows the NEAREST bone. Could be legit wrong-part refusals. Needs a hit-bone print if anyone re-opens it.

## 4. Slice design: DribbleEngage (handoff)

1. Red test first: a trapper whose granted touch leaves the ball at rest reaches the act4 start within K ticks of the trap end (P+0xAC8 == 4 with act4 params), or the holder becomes retainer with the ball moving. K from the clip's remaining frames post-contact.
2. Run the end chain in the window at trap end: the tick already calls `0x14079E5C0`; verify the beyond-early-return path is reached when the clip finishes (playing[] empties via TrapFactsOf when `frameNum >= frameCount - 1`) — if it early-returns forever, that is the first bug.
3. Translate + bounded-prove the dribble tick `0x1407B37D0` (closure from its direct callees; synthetic states over a real trap-end state; label them) and the ball kick application `0x140870A40`.
4. Wire `RunDribbleTicks` per frame for P+0xAC8 == DRIBBLE men; extend P-ownership discipline; humanoid plays the dribble's issued motion.
5. Mutations (kill the dribble runner; force switch to MOVE only), `GF_PES_SURVEY` empty, smokes, `GATE_TIER=branch`.
