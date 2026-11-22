# CutsceneAnchor12 (CutLibrary): the fill chain is proven piece by piece (name match and fill loop, bit-exact on synthetic states over seq321 with the install's object bytes); the group loader objects and their boot caller are mapped but not decoded, so the fill is not wired and the wide test is still RED.

Branch `CutsceneAnchor`, pushed head `aef12c1fab081e2002fcc6cba22fab5c6aad841d` (`git ls-remote origin CutsceneAnchor`), on `e647d824` (the note correction) and `4d33baa3` (the parser). Labels: CONFIRMED = read or run this round; INF = inference; OPEN = named blocker. Static work plus synthetic proofs; live reads only for the checks noted. PES was relaunched during the round (pid now 1502356; the earlier pid 678718 is gone).

## 0. Outcome

- PROVEN this round (snapdiff --bounded, SYNTHETIC states over seq321, install's object bytes):
  - name match `0x141E5CB40`: 3 of 3 calls bit-exact, 0 mismatches, own-block 0.90 (create, already-filled, no-match cases).
  - fill loop `0x141E5D560`: 3 of 3 calls bit-exact, 0 mismatches, own-block 1.00 (two items created or found, and a no-match failure).
- Both records are in `tools/dev/translate/results_core.json` with `closure_cutname.json` and `closure_cutfill.json`. The layer was regenerated (5778 functions before the fill record; the regeneration for the fill record is the last step of this round, see section 7).
- NOT wired: the GF fill needs one group loader object per constant bin, with its name and kind, and those come from the boot caller chain (section 3), which I did not decode. So GF has no faithful source for the group list yet.
- Wide test `GoalAnchorKindTwo.WideChannelScorerTakesTheElseArmAndEqualsPes`: still RED (unchanged, section 6).
- Correction: CutsceneAnchor11 and before read the object's vtable at `0x142B48C88`. The vtable is `0x142B48C80`; `0x142B48C88` is its slot +8, which holds the fill `0x141E5D560`. The earlier "no reference to the vtable" was the wrong address (CONFIRMED: `lea rax,[rip+0xceb35b]` at `0x141E5D91E` resolves to `0x142B48C80`).

## 1. The name match 0x141E5CB40 (CONFIRMED)

- Arguments (CONFIRMED from the decompile and the call sites in the fill): `rcx` = the cut-library header (`[0x1437F4808]`), `rdx` = the output index pointer, `r8` = the item's object (the WESYS object bytes, what the factory's loader reads as `rdx`), `r9` = the flag, `[rsp+0x28]` = the item's name (char*).
- Returns `al` = `movzx eax, bl` (CONFIRMED from the tail of the function): 1 when the item is found or created, 0 when no basename matches.
- Behaviour, as traced: the basename (directory and extension stripped) is matched case-folded against the 234 names. On a match with a null slot, it creates the entry: `0x141E5E0A0` (the 0x18-byte entry), `0x141E5E210` (the factory of the name's record, then the node loader `0x141E6C280` through the node's vtable slot +0x10 with the object), and stores the index in `*rdx`.
- PROOF (CONFIRMED by run): `create` (slot 113, `demoarea_st001`, cleared; the create path ran `0x141E5E0A0` 1x and `0x141E6C280` 1x), `exists` (slot 113 as PES filled it), `nomatch` (`nomatch.o`, returns 0). 3 replays, 0 mismatches, every written byte, `rax` and `xmm0` equal.
- Closure (34 members after excluding native services): the factory `0x141E6C240`, the constructor `0x141E6C100`, `0x141E6C280`, the resolver `0x141E823E0`, the entry init, string and list helpers, and the allocator `0x1403FB410` (native service in production).

## 2. The fill loop 0x141E5D560 (CONFIRMED)

- Receiver: the group loader object (vtable `0x142B48C80`, slot +8 = `0x141E5D560`). Its `+0x98` is the group kind (read by the fill and passed to `0x141E5BA70`).
- Preconditions (CONFIRMED from the decompile): both `[0x1437F4808]` (the cut library) and `[0x1437F4800]` (the group table) are non-null.
- The list (CONFIRMED from the accessors `0x1414A9800`, `0x141030AB0`, `0x1414A98D0/9900/98A0`): a self-relative table. `u32 count` at `+0`, `u32 recordsOffset` at `+4` (relative to the list start), then `count` records of 12 bytes: `+0` argOff (the object pointer, relative to the list start), `+4` flag, `+8` nameOff (the name, relative to the list start). A zero offset means null. (A first attempt had name and arg swapped; the trace showed the name match called once and the fill failing at the first item, which is how that was found.)
- The loop: for each item, `0x141E5CB40(header, &list[idx], arg, flag, name)`; a zero result frees the list and stops (the failure path, `0x141E5DDF0`); otherwise continues; at the end `0x141E5BA70(group table, kind, list)`.
- PROOF (CONFIRMED by run): three cases as above (two created or found, two found, and a no-match second item). 3 replays, 0 mismatches, own-block 27/27.
- The fill's list allocation is `0x1403FB410` (native service).

## 3. The group loader objects and their boot caller (mapped, not decoded: OPEN)

- `0x141E5D700(name, kind)` (CONFIRMED decompile): allocates a `0x98`-byte holder, zeroes it, calls `0x141E5D8C0(holder, name, kind)`, which allocates the `0xA0` loader object, sets its vtable `0x142B48C80`, copies the name (at most 0x80 bytes) to `+0x10` through a copy function pointer `(_DAT_142530660)`, and stores `kind` at `+0x98`. The holder's `[0]` is the object.
- Callers of `0x141E5D700`: `0x141E5BF7C` (appends the holder to a vector: `FUN_141e5bf7c`) and `0x141E5C274` (a loop over a list of holders: `FUN_141e5c274`, which then calls `0x141E5D4E0` and frees the holder). The name and kind constants come from the caller of those two, which I have not decoded.
- Live check (CONFIRMED): no live object has the first word `0x142B48C80` in pid 1502356. The loader objects are transient: created at boot, freed after the fill. So the live value of the kinds cannot be read directly.
- The group table `[0x1437F4800]` (234 slots, built by `0x141E5BAF0` → `0x141E5B900`): its slot `kind` is notified after each group's fill (`0x141E5BA70`). The kind to slot mapping is part of the boot caller (OPEN).

## 4. Wiring (not done)

- The plan that the proofs support, in GF: (a) read each `common/match/constant/*.bin` WESYS container through `PesVfs::Read` and `UnwrapWesys`; (b) build the list blob (section 2 layout) from its object table (`count`, `table` at `+4`, entries `(dataOff, size, nameOff)`; the list needs `(argOff, flag, nameOff)`, so `argOff = dataOff` and the name is the object's name); (c) create the group holder and object (translated `0x141E5D700` if its copy function pointer `_DAT_142530660` and `0x1414A8790` are modelled, or a native object with the same fields); (d) call the translated fill.
- Blocked on: the group names and kinds (section 3). Without them GF's group objects would have wrong kinds, so the notify `0x141E5BA70` would reach the wrong slot. GF may build the library through the fill with kinds once section 3 is decoded.
- The wide test is not wired in this round, so the key (`[T+0x1C]`) still comes from GF's zeroed time-scale object. The key for the case is the capture's (seq204's `0x71`); the wide test loads the capture's pre-state only in the test's own facts. No GF change in this round.

## 5. Checks (CONFIRMED)

- Records: `results_core.json` (name match and fill; the JSON parses); `closure_cutname.json` (34 members), `closure_cutfill.json` (53 members after excluding the 8 heap services of `stubs.json` native_services: 0x1403FBC10, 0x1403FBCB0, 0x1403FBD00, 0x1403FBEB0, 0x141B34CB0, 0x141B34D80, 0x141B34C30, 0x141B34DB0).
- Layer regeneration after the name-match record: rc 0, 5778 functions (3767 recorded bit-exact, 2011 closure).
- The fill record regeneration and `--check`, and `ledger.py --check`: see section 7.

## 6. Wide test (CONFIRMED by the last run, CutsceneAnchor10; not rerun this round: no GF change)

- RED, 9 EXPECT failures for DC 5/100/161: `T[0]` -41.5502167 vs -41.6211472, `pos[0]` 0 vs -41.5502167, `pos[2]` 0 vs 34.

## 7. Open items (numbered)

1. Decode the boot caller of `0x141E5C274` (its loop over the holder list) and the constants it passes (names of the constant groups and kinds). Blocks the wiring.
2. Find the caller of the fill: the dispatch `call [reg+8]` on a group loader; the group table `0x141E5BA70` notify is the other side.
3. Wire the fill at the world build (section 4), then the wide test. The key: GF's own stadium choice through the PES field, still OPEN (CutsceneAnchor11, item 1).
4. The loader slot +0x10 of the constant-object vtable `0x142B502C8` (`0x15B35E0C0`), needed for the 0x30 constant objects (the animeAging/ball group). Not decoded.
5. The other factories of the 234 names (84 `0x141E6C240`, 34 `0x141E5F430`, 21 `0x141E621B0`, 15 `0x141E7C240`, singles).
6. The writer of `[T+0x1C]`.
