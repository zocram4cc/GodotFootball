# pes-port-Kind8Lane (round 3 status): merged Integrate; ball extent set to 0x52DA (uncommitted, unbuilt); contact-struct caller chain decoded; GF side not started; disk below floor

Status (as of this round):
- Pushed: origin/Kind8Lane at 8a287d31 (ball-sync 0x140872C10 and snapshot copy 0x140430C30 proven on pid1971636; gen --check green on that tree).
- Local, not pushed: 1226ac31 = merge of origin/Integrate 8fa69b21 (results via json_union_merge; layer regenerated, rc 0, 10968 functions, 3902 recorded bit-exact). Not yet gen --checked on the merged tree.
- Uncommitted: src/sim/onthepitch/pesworldrecord.hpp: kBallRecordBytes = 0x52DA (was 0x2960), with kBallSnapshotOffset = 0x296C, kBallSnapshotCellOffset = 0x52D8 and two static_asserts. NOT built (disk is at 136 GiB, below the owner's 150 GiB floor; Main told).

## 1. Ball object size (step 1): partial, lower bound only
- Allocation NOT found. The only ctor-side allocation I can cite is 0x14046DC00 -> FUN_14029D370(0x29F0), setter FUN_140A571F0 on obj+0x80 (CONFIRMED). That object does not contain the live ball: the vtable 0x1425A0B48 is absent from capture pid1971636's memory, and B-0x80 holds 0xFF00000000, not a vtable.
- Extent cited (lower bound): B's own fields end at B+0x296C (the copy 0x140430C30 writes B+0x2968..0x296B); the snapshot N = B+0x296C (0x1404335B0 `lea rdx,[rcx+0x2970]`, rcx = B-4, CONFIRMED disassembly); the cell B+0x52D8 is u16, zeroed by 0x141F4CB6F (`mov word [rbx+0x52d8],0`, CONFIRMED decompile). So the object is at least 0x52DA bytes. The exact allocator size is OPEN.

## 2. Contact struct S (step 2): caller chain only
- 0x140872C10's first argument S: `lVar12 = FUN_1404335E0(S+0x4E0)` gives B; the contact fields are S+0x288..0x2A0.
- Its caller is FUN_140425A90 (0x140425A90; decompile in scratch dec_425a90.txt): `S = *(P+0x20)` with P its argument; it calls the arm FUN_1408722C0 (writes S-side p+0x298 = 3), FUN_140872570, func_0x140872200, then FUN_140872C10(S).
- FUN_140425A90 has NO direct callers and no data references in the image (E8 and qword scans empty). It is reached indirectly; the caller is OPEN.
- Writers of S+0x288..0x2A0 (the contact struct's own writers): NOT decoded. The disp scans are not enough (too many objects share these displacements).

## 3. Rig: decision captures (pid1408046, Main's launch)
- seq25836 (kind8_140A19290, unpacked, the only one unpacked): replays BIT-EXACT (prove_bounded, 1 call, 0 mismatches; own block 4/259). Its ball: B.2918 = 0 (gate off), B.291C = 3, B.2920 = 1, B.290C = 0xC, B.2914 = 6; MI.116C = 2 (stopped), MI.1168 = 1, MI.173C = 9. This is a keep-path or stopped case, not a loose-ball exit.
- The other 18 pid1408046 captures are packed. Unpacking them needs disk: do it one at a time after the floor is restored.

## 4. Not done
1. Build and test of the size change; gen --check on the merged tree; push 1226ac31 and the size commit.
2. The contact-struct writers and the FUN_140425A90 caller (indirect dispatch: trace the vtable or the calling trap object).
3. GF side: the sync and snapshot on GF's ball, the producer in the event stage, red-first kind-8 ring test, counter-B feed, race test, numbers.
4. Loose-ball exit of the decision: a true return needs one of the pid1408046 captures with MI.116C == 1 and a loose ball (look for it when the floor allows).
