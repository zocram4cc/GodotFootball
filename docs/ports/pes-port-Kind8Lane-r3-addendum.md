# pes-port-Kind8Lane addendum 4 (round 3): the owner's per-tick chain is decoded; the holder reset is NOT yet tied to S+0x288; store scanner landed

## 1. Owner's per-tick chain (CONFIRMED decompiles: dec_owner.txt, dec_helpers.txt, dec_424edc.txt, dec_424f94.txt)
- Slot 4 of vtable 0x1425988B0 = FUN_140424F70(O, a2): calls FUN_140A49C50(O+0x30), then FUN_140424D70(O, a2), then clears O+0x48/0x70/0x98/0xC0/0xE8/0x138/0x110 and the 0x16-entry table at O+0x160 when the flag bytes at O+0x4B8.. are set (flag O+0x4E0 cleared after).
- FUN_140424D70(O): calls FUN_140A0C3E0(O+0x10, O+0x110) and FUN_140425E20(O); reads MI = *(G+0x270), G = *0x1436F3DC8, and MI+0x11B0; branches on the play state; in the in-play branch (MI+0x116C == 1 path) it does the reset below, then calls the thunk FUN_140424EDC -> the driver FUN_140425A90 (S = *(P+0x20)).
- Reset (CONFIRMED decompile, in the play branch when the gate predicates in FUN_140424D70 hold): `puVar6 = FUN_140424F00(); *puVar6 = 0xFF; *(u8*)(puVar6+1) = 0; *(u64*)(puVar6+2) = 0; *(u64*)(puVar6+4) = 0; *(u64*)(puVar6+6) = 0; puVar6[8] = 1.0f`. The record is 0x24 bytes; its +0 holder word is 0xFF.
- FUN_140424F00(R): lazy accessor: if R's byte +8 is clear, calls vtable slots R->+0x10 and R->+8, runs FUN_1404258B0 (closure member, proven in kind8ballsync set? it is a closure of the sync), sets the byte; then returns R->+0x20 if set, else (*(R->+0x10)) + 4 (stored). So puVar6 = (*(X+0x10)) + 4 for the record object X = R[2].
- FUN_1408709B0(O): zeroes B-relative fields through FUN_1404335E0(O+0x4E0) (B+0x24.., B+0x40.., B+0x4C, B+0x58.., B+0x74..) and the 0x500/0x4F0 job. It writes B, not S.

## 2. What is NOT resolved
- The reset record puVar6 is NOT the contact struct at S+0x288 as far as the captured fields show: captured S+0x28C = 0xFF but the reset writes byte 0 at puVar6+4, and captured S+0x2A0 = 0 while the reset writes 1.0f at puVar6+0x20. So puVar6 is a different record (X's record at +4), not S+0x284. Its identity (X, and whether X is S or S's owner record) is OPEN.
- Writers of S+0x288/0x28C/0x290 and the gate bit 4 are still NOT found. The arm (FUN_1408722C0) writes S+0x294 bit 0 (or dword [rdi+0x294],1 at 0x14087237A, confirmed by the new store scanner) and S+0x298 = 3; FUN_140872570 clears bit 0. The sync copies S+0x288/0x28C/0x290/0x294 -> B+0x290C/0x2910/0x2914/0x2918.

## 3. Tools landed
- stores_scan.py (scratch): given entries, disassembles each .pdata chunk at the entry and lists memory WRITES with displacement 0x288..0x297 (excluding rsp). Verified: finds 0x14087237A in FUN_1408722C0. Caveat: multi-chunk functions need their extra chunks (the sync's entry chunk is a jump stub).
- refs_code2.py: fixed code-reference sweep (rip-relative and imm).

## 4. Next (ordered)
1. Identify X: the object whose +0x10 holds the record; compare X against S (= *(P+0x20)) in the captures: read X and X+0x10 in pid2282538's driver captures (in place) and see whether X+0x10+4 == S+0x288 or some other address.
2. If X+0x10+4 != S+0x288, scan the owner's callees FUN_140A0C3E0 / FUN_140425E20 / FUN_140A49C50 with stores_scan.py for 0x288..0x297 writes.
3. Gate-off captures (sync_140872C10 with (S+0x294 & 0x10)==0 in play; owner_140424F70 EVERY 50 in play) when they land in pid2282538: prove the owner's tick on them (replay), then translate.
4. GF side: not started.
