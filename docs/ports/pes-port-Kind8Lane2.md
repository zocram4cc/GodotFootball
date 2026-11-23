# pes-port-Kind8Lane2: kind-8 gate chain decoded; driver proven; holder path and GF side not done (Kind8Lane, 23-11-26)

Status: pushed `Kind8Lane` a2f340c4 (ls-remote confirmed). Working tree clean; nothing new committed after it. GF side NOT started in code (no feed, no kind-8 ring test, no counter-B, no race tests, no ALiveBall / PesStallQuick / PesPassTarget numbers).

## 1. Owner chain (CONFIRMED, decompiles in scratch dec_owner.txt, dec_425a90.txt, dec_424f94.txt, dec_gate1.txt)
- Owner O = singleton 0x1436F4090 (ctor FUN_14042461C). Slot 4 of its vtable = FUN_140424F70: FUN_140A49C50(O+0x30), FUN_140424D70(O), clears of O+0x48..0x138 under flags O+0x4B8.
- FUN_140424D70(O): FUN_140A0C3E0 (translated), FUN_140425E20 (untranslated). In the play branch: FUN_1408709B0, the holder-record reset (FUN_140424F00 record: +0 = 0xFF, +0x20 = 1.0f), then FUN_140425A90(O) via thunk 0x140424EDC.
- FUN_140425A90(O) (driver): S = *(O+0x20). Accessor on S+0x558/560/570, FUN_1408722C0 (arm: S+0x298 = 3, S+0x294 |= 1), FUN_140425010(O), FUN_140872570(S), FUN_140872200(S, state), FUN_140872C10(S) (sync: B = FUN_1404335E0(S+0x4E0)).
- FUN_140425010(O): per-player loop (both sides, 11 slots, flag +0x10d, radius test of the ball). `if (iVar8 < 1) return 0;` before the holder call, so the holder path runs only when a candidate man is within radius. Holder call: FUN_140870A40(S, slot, pos, vel, rec, kind==0xd) guarded by `iVar14 != 8`.

## 2. Gate writers (CONFIRMED structure)
- FUN_140870A40 (translated closure member, src/pes/translated/closure/pes_fn_140870A40.cpp) writes S+0x298 (2/1), S+0x288 (slot), S+0x28C (slot), S+0x290 (class byte), S+0x293, and at L_140870E4B `S+0x294 |= 0x13` (bit 4 = in-play holder gate).
- Bit 4 is cleared only by whole-word/qword stores; the clearing writer is OPEN.
- FUN_1408722C0 sets bit 0; FUN_140872570 clears bit 0 (and 0xfffffffe at 0x1408725B0); FUN_14086F608 clears bit 1.
- Live (pid 2282538 and 2586283, read-only pread via sudo -n, in play): S+0x288/28C/290 change (0x14, 0x11, 0x15, 3, 6); S+0x294 stays 0 across 60 samples at 0.5 s, so bit 4 is set and cleared inside one tick. Sampling cannot see it.
- Reset record Y is a separate heap object (live Y = 0xabeff34), not S.

## 3. Proofs (CONFIRMED, snapdiff --bounded via prove_bounded.py)
| root | real calls | replayed | mismatches | own block | closure | state |
|---|---|---|---|---|---|---|
| 0x140425A90 driver | 12 (pid2282538 drv_140425A90) | 12 | 0 | 45/65 | closure_kind8owner.json (47) | COMMITTED a2f340c4 (gen --check green on tip, 10,975 functions) |
| 0x140425010 owner loop | 1 (pid2586283 a40caller seq093) | 1 | 0 | 19/95 | closure_kind8a40 (members: 0x140A48A30 only) | proven in scratch, NOT merged: merging changes the layer and the layer was not regenerated; reverted so the tip stays gen-check-clean |
- The holder path 0x140870A40 was NOT entered by any of these 13 real calls (the candidate test failed every time). Its proof needs a gate-on capture: sync_140872C10 armed by Main with `(d(rcx+0x294)&0x10)!=0` (12, FULL), in pid2586283; not yet analysed.

## 4. Next steps (ordered)
1. Re-run prove_bounded on 0x140425010 with --merge, then gen_translated (~11 min) and gen --check (~10 min); commit with the regenerated layer; push only if check is green. (The proof is reproducible from seq093 in live2/pid2586283.)
2. Prove 0x140870A40 on the gate-on sync captures (pid2586283, sync_140872C10 predicate): the proof of the holder writer and of bit 4 at sync entry. Check the clearing writer of bit 4 against these captures.
3. Translate and prove the rest of the owner chain: FUN_140424F70, FUN_140424D70, FUN_140424F00, FUN_140425E20, FUN_140A49C50, FUN_1408709B0, FUN_140872170, FUN_1404258B0.
4. GF side (not started): run the owner tick in GF's sim at the PES call order; sync and snapshot on GF's ball record (kBallRecordBytes 0x52E0 lower bound); kind-8 producer in matchevents.cpp's event stage (ballState is hard-coded to in-play at matchevents.cpp:189; GF has no B+0x2918 bit 4 source); red-first kind-8 ring test (capture-driven); counter-B feed; race tests; numbers. Must port the writer of bit 4 (rule 4: port subsystems, not fields) — do not set it to make a call pass.

## 5. Blockers and notes
- Disk: <workdrive> was at the 150 GiB floor during this round; Main is moving old launches off it.
- check_pii: not re-run this round (no new commit; a2f340c4's files were grepped clean for <workdrive> and <home>). Main to run tools/check_pii.sh from the main checkout.
- 0x140870A40 cannot be hooked directly (rip-relative operand in the displaced window); hook its caller 0x140425010 or the sync gate bit instead.
- Scratch: all unpacked captures deleted; scratch dir is 9.7 MB (decompile text and logs).
