# MB-H (match setup and tick order): Wave 1 notes (Wave1MBH)

Static evidence only (Ghidra bridge; raw image scans of pes_full.bin, file offset = VA - 0x140000000). Nothing read from the live game.

## 1. Owner of the team tick and its vtable slot 2 (H3 caller hunt)

- Owner H: `FUN_14041e4d0` (ctor, VA 0x14041e4d0). Vtable 0x142597580; slot 2 (+0x10) = 0x14041ecf0 (team-tick routine: loops both sides, calls `FUN_1404f6c30` per side; tail 0x14041edd7 is mid-function).
- H is stored at match `+0x788` by `FUN_140418940` (store at 0x140418de1), and the ctor writes the global `_DAT_1436f3f98 = H`. A RIP-relative scan of the whole .text finds only those two writes (no reader of the global).
- H is registered into a listener: `FUN_140418940` MatchSub block creates `L = FUN_14042fe20(...)` (vtable 0x14259b1a8, final vtable) and calls `L->vtbl[+0x18]` = thunk 0x14041bcf0 (`add rcx,0x30; jmp 0x140a8f460`), i.e. push H into `L+0x30`. `FUN_140a8f460` AddRefs H (`lock xadd [H+8]`, FUN_1403db250) and push_backs H (FUN_140a8f730).
- **Caller of `[H + 0x10]`: `FUN_140a8eff0`** (VA 0x140a8eff0). It walks the vector at its first argument: `mov rcx,[rcx+rdx*8]; mov rax,[rcx]; call [rax+0x10]` (0x140a8f029..0x140a8f036), i.e. `H->vtbl[2](H, msg, xmm2)`.
- Its callers, by the listener's tick: `L` slot 2 = `FUN_140433760` (on record type 2 it calls `this->vtbl[+0x20]` = `FUN_1404334e0`). `FUN_1404334e0`: if `(this+0x28)->vtbl[+8]()` is true it tail-calls `FUN_140a8eff0` (team tick, after timing bookkeeping); if false it tail-calls `FUN_140a8f0f0` (slot-3 broadcast; H's slot 3 is the stub 0x141794410, so no team tick).
- Listener objects are created by `FUN_14042fe20` (MatchObjectListener, names "MatchMainListener" and "MatchSubListener"); `+0x28` is set by `FUN_140432cd0(listener, index)`.

## 2. The ten steps (FUN_14042b9f0; match listener class vtable 0x14259a350, slot 2)

Record type (low 7 bits of the record header) -> step:
- 0x1050012 -> step 1 `FUN_140426a90` (callees: FUN_140a20020, FUN_14041b790).
- 2 (the tick) -> step 2 `FUN_1404267a0` (FUN_140a49c50, FUN_140426950); step 3 `FUN_1404267f0` if `+0x50` (event stage: detector loop `FUN_140a0d3f0`, K=12); step 4 `FUN_140426860` if `+0x51` (`FUN_140a0d520`); step 5 `FUN_140426760` when neither flag and the `+0x28` predicate holds (`FUN_140a0d190`, which calls `FUN_140a3e140` the per-pass log reset, and `FUN_1404582c0`); step 6 `FUN_140426810` (`FUN_140a484f0`).
- 0x105000f -> step 7 `FUN_140426940` (FUN_140a3eb50). 0x105000e -> step 8 `FUN_140426a80` (FUN_140a3ebf0). 0x105002c -> step 9 `FUN_140426750` (no FUN callees). Other types -> step 10 `FUN_140426cd0` (message switch: flags and phase; stamina-live note).
- Record 0x1050001 and 0x1050029 call the match object's vtable slot +0x50 (not decoded).

## 3. Placement of the team tick (INF, medium)

Within the type-2 tick record: steps 2 -> 3 (event stage) -> 4 -> 5 -> 6, then the team tick (both sides), delivered by the MatchSub listener in the same record. So the team tick runs after the event stage: K=12 (step 3) reads the latch of the previous tick (soft edge; see PoseVeto §17).

OPEN: the order in which the event hub dispatches type 2 to the match listener versus MatchSub. The hub (`FUN_141b3edd0`, `FUN_141b3ceb0`) calls, per record, `1020[T]` (channel 0), `820[T]` (channel 2) or `20[T]` (other), then `1820[T]`. Which listener sits in which array was not decoded (the Register body is not identified). Registration order in `FUN_140418940` puts the match listener (collision_init, `FUN_14042b100`) before MatchSub, so if the hub dispatches in registration order the team tick follows steps 2-6.

OPEN: the event pump is `FUN_141b3ea30` (called from the pump's vtable slot 6, 0x142ac1dd0 -> 0x141b3dd80). It drains one queued event list per call with a 2-frame lag (hub+0x4190). The order of record types within a frame is set by the producers of those lists (not decoded).

## 4. Referee, ball and mental-image placement: OPEN

- Steps 2, 4, 5, 6, 7, 8 are callee-mapped only (named helpers above; bodies not decoded).
- The whistle `FUN_14050e770` is called from judge-side functions (FUN_14050c490, FUN_14050e020, FUN_14050e110, FUN_14050e320, FUN_14050cc40), not from the tick chain. The referee's tick position is not placed.
- The MA (mental image) build is not located in the tick chain.
- Therefore the GF Match::Process reorder cannot be justified from decoded PES sites yet; it is not applied in this wave.

## 5. H1 (match-setup rating term)

- Decider: `FUN_14051e970`, minute-0 block: `d = strength(own) - strength(opp)`; `d > 0.9` (0x14258c260) takes the stronger row, `-d > 0.9` the weaker row (match-management 1.3). Already ported in pesstrategy.
- Strength input: `G+0x2ad0 + 0xc404 + side*0x2804` (INF: team rating). Writer NOT found: no store with displacement 0xc404 (7 loads only); 96 `imul ...,0x2804` sites are team-block accessors and were not traced to the match-load writer.
- GF port: the setup strength is a structure (`Match::teamSetupStrength`, written once after `InitPlayers`), read by the decider through `StrategyInputs::ownStrength/oppStrength`. Value: `pes::SetupStrength` = mean starting-eleven overall x 100 (ponytail, the stand-in the old per-tick lambda used).

## 6. Gates (M1, D5, T1, T3, C1, C2)

- T3: rating input is structural now; modes 1-6 and the plan queue remain.
- M1 (tick order), D5, T1, C1, C2: still blocked on sections 3-4 (step bodies, dispatch order, referee/MI placement) and on MB-A/B/C/E.

## Next steps (for the orchestrator)

1. Name steps 2, 4, 5, 6, 7, 8 (callees FUN_140a49c50, FUN_140a0d520, FUN_140a0d190, FUN_140a484f0, FUN_140a3eb50, FUN_140a3ebf0).
2. Decode the hub `Register` (which array a listener lands in per type) to settle match-listener versus MatchSub order.
3. Find the referee's and the MA build's per-tick callers.
4. Writer of `G+0x2ad0+0xc404+side*0x2804`: trace from the match-load writer (the `0x2804` stride sites).

## 7. Delivered (branch Wave1MBH, commit a2b08b8, base 453640f)

- H1 structure: `Match::teamSetupStrength` (written once after `InitPlayers`), `pes::SetupStrength` (ponytail stand-in: mean starting-eleven overall x 100), decider takes `ownStrength`/`oppStrength` and forms `d = own - opp` (FUN_14051e970 minute-0 block).
- Mutation checks (each caught): divide by n+1 (SetupStrength test); sign flip of d (2 decider tests); own-only d (2 decider tests); Match write zeroed (sim integration test). All reverted.
- Gate: ctest 712/712 passed; 2 standing install skips (VfsInstall.NestedOriginalsSurviveWithoutDownloads, EditSaveInstall.DecryptsAndMatchesTheRunningGame).
- Smoke seed 7, 20 min: base 453640f and H1 both `[determinism] 45b5c9894844c576`, HOM 3 - 4 AWA. H1 is determinism-neutral.
- Not done: Process reorder (no decoded PES site for the referee or MA placement; step bodies 2/4/5/6 and the hub's per-type dispatch order open). Gates: none fully unblocked; T3 advanced (rating input structural), D5/T1/C1/C2/M1 still blocked on their other structures and on sections 3-4.
