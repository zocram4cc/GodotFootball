# PressLane2, tick chain round: the tick proof is blocked by a translator bug (switch table decoded as code); nothing landed

Branch `PressLane` at `8f0d8192` (pushed, confirmed). This round landed no code: the walk is still in `_BallControlCommand`, RunActionPress is not wired, and the repo tree is clean.

## What was established

- **No guard on the tick's init.** The init block `0x1409290D0..0x140929108` is straight-line code (no branch targets it), so the zeroing of A2+0x38/+0x40 runs on every tick.
- **The tick runs.** Live (pid 2282538), A2 slot 21's stamp (A2+0) changed 35 times in 5 s; ctx stayed `0x32EC15D0` and owner `0x3C8B0320` in ~2.0M reads (sample interval ~2.4 µs, so a sub-µs zero window cannot be ruled out).
- **Source of the restored values.** `holders` on seq3076: owner at `X+0x17D8`, ctx at `X+0x17E0`. The tick copies them into A2 through a path not yet identified (no direct `0x17D8/0x17E0` displacement in the 13 calls between the init and frame 6, first 1600 bytes each).
- **Tick closure.** One tick call enters 252 distinct callees; all 252 are already in the layer. The static closure of `0x140928FC0` is 2,451 functions (mostly menu and pre-match code), so proving the static closure is not the route; capture-bounded is.
- **Proof attempt (`prove_bounded 0x140928FC0`, seq3076).** Root spec created (`closure.py --members-file`, 266 members: the 252 entered callees plus the 54 reached from `0x140928710`). The run still fails: `translate 0x140928710: KeyError('ss')`.

## Blocker (CONFIRMED by disassembly)

`0x140928710` contains a switch. Its dispatch block at `0x140928EC8..0x140928EF6`:
- `lea rdx, [rip-0x928EE2]` (rdx = image base 0x140000000),
- `movzx eax, byte [rdx+rax+0x928F84]` (byte index table at `0x140928F84`),
- `mov ecx, [rdx+rax*4+0x928F74]` (dword offset table at `0x140928F74`),
- `add rcx, rdx; jmp rcx`.

The tables sit INSIDE the function's `.pdata` extent, after an unconditional `jmp` at `0x140928F6D`. `xlate.py` decodes the table bytes as instructions, so it emits `mov ss, word ptr [...]` at `0x140928F75` (a table entry, not code). `reg_parent('ss')` has no entry, hence the KeyError. The fix belongs in `xlate.py`'s switch-table handling (the entry count and the extent: table bytes must not be decoded as code). Not attempted this round.

## State

- Repo: clean at `8f0d8192`. The closure run's artifacts (closure-pressttick.txt, 8 specs in `specs/trapaction`, a universe.json rewrite) were reverted; copies kept in `press2/tick_specs/`.
- Scratch: the unpacked tick capture is deleted; disk 151 GiB.
- Hooks: the tick replay config with displaced bytes from the image (`pt_140928FC0` 19 bytes, `pt_14092A340`, `pt_1405140E0`, `pt_140961FB0`) is in `press2/hooks_tick_replay.cfg`; the current `hooks.cfg` does not carry the pt_ tick/loop/man entries.
- Not run: ALiveBall, PesStallQuick, ankle 9/9, PesHang, gen --check (no change to commit).

## Next, in order

1. Fix the `xlate.py` switch-table extent (table bytes not decoded as code; the case count from the byte table's max index). Red test: a switch whose table lies in the extent translates with no `mov` to a segment register. Then retry `prove_bounded 0x140928FC0` with `hooks_tick_replay.cfg` on seq3076 (unpack first; disk needs >= 150 GiB headroom).
2. Prove the tick over the three pt_140928FC0 captures (one at a time for disk), then 0x14092A340, 0x1405140E0, 0x140961FB0 as roots or in its closure.
3. Then: the copy from X+0x17D8/0x17E0 is inside the proven closure; run the tick per X per frame where PES runs it; RunActionPress falls out (driver, then ActionPress); delete the walk in the same commit; red test; numbers; gen --check; check_pii; push.
