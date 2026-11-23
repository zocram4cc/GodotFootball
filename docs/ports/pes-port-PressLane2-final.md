# PressLane2 final state: pushed driver proof (8f0d8192); the switch-table decode fix is saved as a patch, not committed; ActionPress not wired

## Landed and pushed
- `origin/PressLane` = `8f0d8192` (confirmed by ls-remote). Contents: the driver `0x1405141F0` proof merged (3 real calls, 0 mismatches, own-block 87/139), `closure_pressdrv.json`, and the regenerated layer (10966 functions). gen_translated --check was green on this tip.
- check_pii from the main checkout: rc 0.

## Not landed (reverted from the worktree)
- Switch-table decode fix: `pdata.py` records each resolved switch's byte map and dword table as data, and the linear walk stops at them. Test `SwitchTableInTheExtent` (red on the original, green with the fix); the 3 existing switch tests stayed green.
- Saved as `press2/pdata_switch_fix.patch` (scratch, 170 lines). Reverted in the worktree because the regeneration was not finished: the gen run exceeded its 300 s limit and was killed, so the layer was NOT regenerated and gen --check is NOT green with the fix. Expected effect: 18 existing functions change (2 recorded: `ai/pes_fn_1404F0E50`, `gameplay/pes_fn_140575510`; 16 closure). The change removes junk decodes of in-extent jump tables (checked on 1403939D0: a table at 0x140393B68 decoded as junk, incl. a RIP reference to 0x15B3974B8).
- Next for this fix: apply the patch, run gen_translated (no --check) in background without a 300 s limit, commit pdata.py + test + layer in one commit, re-prove the 2 recorded functions on their original captures (pid1858437/pid1724101 entries from their results records), gen --check, push.

## Open, not done
- Tick proof `0x140928FC0`: blocked on the switch-table fix above (`0x140928710` translate failed with `KeyError('ss')`; with the fix it translates). Root spec and 266-member specs were generated and then reverted (copies in `press2/tick_specs/`).
- Tick ctx/owner source: A2+0x38/+0x40 are copied from X+0x17E0 / X+0x17D8 inside the tick; the copy path is not identified. Live reads (pid 2282538): ctx/owner never zero in ~2.0M samples (sampling granularity ~2.4 µs).
- Walk NOT deleted; RunActionPress NOT wired; no red test; no census, PesStallQuick, ankle 9/9, PesHang run.

## Scratch
- `press2` is 27 MB (scripts, disasm, patch, logs). No unpacked captures remain. Disk 150 GiB free at wrap-up.
