# PassLane2 (run 4): the aim's ring point is written by 0x140A76440 into a per-slot record that mirrors the man's ring; the chain runs from the press tick; the writer cannot be replayed yet; nothing ported or landed

**Hooks (Main has them live).** For section 3 I need one more: `0x142106110` (the writer's caller's function) at its entry, any call, EVERY 1, cap 24, FULL, so the event struct `rdx` and the record `rcx` are seen with the caller's arguments. No predicate. Without it I cannot tell which event runs the writer.

**Status.** `origin/PassLane` = `27aee1b94` (ls-remote confirmed): the merge of `origin/Integrate e154ed687` on top of the run-1 red test `7104ed38d`. Gate evidence for that tip: `gen_translated.py --check` passed (3600 s, `gencheck2.log`: 11191 functions, regenerate byte-identical, rc 0); `tools/check_pii.sh` passed (rc 0, from the main checkout). The tip has no behaviour change of mine. No port, no test change, no measurement. ALiveBall, PesStallQuick, census: not run.

CONFIRMED = read this run (capture bytes, asm, decompile, or a direct E8/E9 scan). INF = inference. OPEN = blocker named.

## 1. The writer, its record and its event (CONFIRMED)
- **Writer:** `FUN_140A76440` (`0x140A76440..0x140A76505`, 197-line decompile `dec_w1.txt`). The chained-unwind fragment `0x140A76589..0x140A7691D` (Main's finding, pdata CONFIRMED) holds the vec3 copy `[rdi] -> [rbx+8]` (`dec_76589.txt`).
- **Arguments:** `rcx` = record `O` (writer `param_1`); `rdx` = event `E` (writer `param_2`): `E+0` a pointer to an actor (its byte `+0x40` selects the arm), `E+8` = the **man record `M`** (the writer reads `M+0x55C`, the position; CONFIRMED: `E+8` equals the man table entry in all 8 captures), `E+0x14` double, `E+0x1C` int, flags `E+0x2D`, `E+0x2E`, `E+0x2F`.
- **Record `O` (CONFIRMED from the writes):** `O+2` byte (count-like 0..4), `O+3` byte (a stamp compared with count-1), `O+4` byte **count**, `O+8` vec3 **point**, `O+0x14C` float, `O+0x3F5`, `O+0x3F7`, `O+0x5C8` a vec3. Size > 0x5C8 bytes.
- **What it writes:** arm `iVar7 == 1` zeroes most of `O`, then sets `O+8` from **`E+4` (the event point) when `E+0x2E == 0 || E+0x2F != 0`, else from `M+0x55C` (the man)**. Arm `iVar7 == 2` zeroes `O` and its point. Selector `iVar7 = FUN_140A75C40(O, E, &uStack_88)`, **undecoded** (OPEN).

## 2. Ring equality with the aim source (CONFIRMED on 8 real calls)
- The PASS_GET aim `R+0x894` is `M+8` (the ring accessor `0x140A76240` answers `M+8` for count 0 and 2; `M` = table entry `G+0x1DD8+40*slot`).
- `pid506456` `ringw1_140A76440` (seq206..214, 8 calls, frames 168-173, slots 22, 23, 24): **`O+4 == M+4` and `O+8 == M+8` in all 8**. The caller's neighbouring object holds the slot id (`+4` = 23 and 22, CONFIRMED on two calls).
- The writer writes `O`, not `M`. So for `M+8` to change, `O` must be copied to `M` somewhere. No copy was observed (OPEN). Equality is what the captures show.
- Aim at frame 172 (`pid506456/seq213`, slot 24): `M+8 = (19.5, 0, 35.5)` = `M+0x55C` = `O+8` (count 0): consistent.

## 3. When it is written (CONFIRMED by before/after reads, slot 22)
- Slot 22's `M+0x55C` is static at `(-5.01, 0, -10.0)` over frames 169, 171, 173. Its `O+8 = M+8` is `(-3.28, 0, -23.30)` at 169 and 171, then `(-3.61, 0, -23.36)` at 173, count 2 throughout. **The point changed while the man stood still**, so the source is not `M+0x55C`. It is `E+4` (the event point, moving about 0.3 m in two frames). So the writer ran between frames 171 and 173 on the `E+4` branch. CONFIRMED that a write happened in that window; INF that it was frame 172.
- Slots 23 and 24 (count 0): the point equals `M+0x55C`, the `M` branch.
- `E+4`: not identified (OPEN). The moving point is a candidate for a receiving point or ball sample. Not decided.

## 4. Caller chain and protection status (CONFIRMED)
- `0x140A76440` is called only from `0x142106110` (E8 at `0x1421065E2`, return `0x1421065E7`). `0x142106110` (`0x142106110..0x1421066C2`) is called from `0x140926600` (inside `0x1409257B0`) and `0x140A36E40` (inside `0x140A36B30`). `0x1409257B0` is called from `0x140928FC0` (the press tick, ledger `1409257B0>...`), which GF already runs (`PesPressTick`; `pes_fn_140928FC0` landed on Integrate by the merge).
- **Protection check:** `docs/re/pes-re-holder.md:82` labels `0x142106110` "Denuvo-VM region". The bytes do not support that label for these functions: `0x142106110` lies in `.trace` (plain code; `.impdata` starts at `0x143D14000`), its `.pdata` body has 39 direct `call`/`jmp` instructions and none targets `.impdata`; `0x140A76440` has 2 direct calls, neither into `.impdata`. Indirect calls (vtables) were not checked. INF: the label came from an earlier region note. Owner to confirm before any port.

## 5. Why the port is not landed (CONFIRMED blockers)
1. **Replay blocked:** `frame_trace.py recorded ringw1/seq206 --hooks hooks.cfg` (`replay206.err`) stops at `unmodelled OS export ucrtbase.dll!sqrtf` (import at `0x1415A1780`, called by the writer). A replay is the proof a port needs, so the writer needs a `sqrtf` import model in the harness (a harness change, boundary policy (6); not added in this lane).
2. **Closure:** the writer's direct callees: `FUN_140A75C40` (undecoded), `FUN_140A6C6D0`, `FUN_141E532D0`, `FUN_141E5CA20`, `FUN_140A6E150`, `FUN_140A6E5E0`, `FUN_140A6E6B0`, `sqrtf`. The record `O`'s owner (the caller's array) is also needed.
3. **The decision and the point source:** `FUN_140A75C40` and the `E+4` source are undecoded (sections 1 and 3).
4. **`O`→`M`:** not established (section 2).
5. **Cadence:** section 3 shows the writer runs on an event (`FUN_140A75C40` decides), not every frame, so the run-2 per-frame cadence is not the fix. The GF stand-in for the event point is the current position `M+0x55C`, which is the `M` branch. That is why GF's aim never moves to the ball.

## 6. Measurements this run
None. Earlier numbers stand: aim 43.26 m (baseline); 51.32 m (run-2 cadence port, reverted); `EveryPassGetEpisodeIsWithinPesLiveRange` red on my tip (10 of 30 over 2.94 s; Main reports 6/34 on Integrate `e154ed687`). Not re-run: no change to measure.

## 7. team.cpp:445
Not changed (Main asked for it in the port's commit). It remains false: samples 1..249 are written by `Ball::CalculatePrediction` (`ball.cpp:135-267`).

## 8. Next, in order
1. Main: arm `0x142106110` (any call, EVERY 1, cap 24, FULL) for `E` and `O` with the caller's arguments; keep `ringw1` live.
2. Decode `FUN_140A75C40` and the `E+4` source (which GF object the press tick holds for it: the receiving point or the ball sample).
3. Harness: review and add a `sqrtf` CRT import model (boundary policy (6)), so the writer replays and can be proven.
4. Then port the writer with its caller's branch, red-first on `ThePassGetAimIsTheBallWhileTheBallRests`; fix `team.cpp:445` in that commit; measure PesPassTarget, PesStallQuick, ALiveBall (bounds 479 / 49; M2 caps 599 / 98), and the census.
5. Task 3 (TRAP reach gate): not started.

## Artifacts (scratch `<workdrive>/gf-PassLane-scratch`, not committed)
`dec_w1.txt` (0x140A76440), `dec_76589.txt` (fragment), `dec_ring.txt`, `dec_514940.txt`, `replay206.err` (sqrtf stop), `gencheck2.log` (gen check pass), `pii.log`, `tools/ring_read.py`, `tools/capmem.py`, `tools/pdata.py`, `tools/callers.py`, `tools/scan_ring*.py`. The per-slot comparison (section 2) was run inline; its script is not saved.
