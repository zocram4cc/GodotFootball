# pes-port-Kind8Lane3 (Kind8Lane, 23-11-26, final): 0x140425010 proof PUSHED (87d5588b, origin confirmed, gen --check green); SIGSEGV root-caused (storage artifact); gate-on predicate CONTRADICTED by memory; bit-4 clearer still OPEN; GF side NOT started.

## 1. Pushed: 0x140425010 proof (commit 87d5588b, origin/Kind8Lane = 87d5588b)
- prove_bounded --merge on live2/pid2586283/seq093 (a40caller_140425010): calls 1, replayed 1, mismatches 0, proven True, own block 19/95 (0.2). Matches the earlier 1/1 record.
- gen_translated regenerated (10,975 functions, 3904 recorded bit-exact, 7071 closure). 0x140425010 moved from closure/ to ball/ (recorded). 0x140A48A30 unchanged.
- gen_translated --check on the committed tree: byte-identical, rc 0.
- tools/check_pii.sh (main checkout): rc 0.
- Commit: 7 paths (layer ball/ + closure/, closure-kind8a40.txt, specs/trapaction/140425010.json, universe.json, closure_kind8a40.json, results_trapaction.json).

## 2. The SIGSEGV: root cause FOUND (CONFIRMED)
- The diff main built in the Dati III work dir died at exec (rc 139, no output, gdb "during startup").
- The same binary copied to <workdrive> ran cleanly: rc 0, "1 calls", translated.jsonl written.
- Cause: the binary executed from <media>/Dati III (ntfs3, `prealloc,acl`). Every failing run had its work dir there. Not the hook bytes, not the translation.
- Rerun with the work dir on <workdrive> (snapshot still read from Dati III) gave proven True.

## 3. Gate-on classification of the 11 sync captures: DONE, and it CONTRADICTS the predicate
- Read S+0x280..0x2B0 by streaming `zstd -dc` to each offset (readcap.py), no unpack. S = rcx = 0x39AEF450 in all 11.
- S+0x294 = 0 in ALL 11, including seq13099 and seq15597 (captured after the gate-ON predicate flip). Predicate `(d(rcx+0x294)&0x10)!=0` would have been false at entry on these two.
- Coherence checks pass: owner O = [0x1436F4090] = 0x292BAA90; vtable [O] = 0x1425988B0; slot4 = 0x140424F70; [O+0x20] = S. So the snapshot is a real live object, not stale.
- Consequence: the holder path 0x140870A40 is NOT proven on these captures. Either the hook predicate read a different instant, or the captures were not taken at the hook (sync entry) for these fields. Not settled. Settling experiment: an unpredicated sync probe that reads S+0x294 at entry in the same pause, or the watcher's own log for seq13099 time.

## 4. Bit-4 clearing writer: still OPEN (CONFIRMED structure)
- Static CFG (114 functions, depth 3): every write covering S+0x294 is an OR that sets bits (holder 0x140870A40 `or 4` at 0x140870D8C and `or 0x13` at 0x140870E4B; 0x1408722C0 `or 1`; depth-1 0x140872210 `or 4`, 0x140872240 `or 1`). Clears: 0x140872570 `and 0xfffffffe` (bit 0), 0x14086F608 `and 0xfffffffd` (bit 1). No direct clear of bit 4.
- Indirect callees resolved on the gate-on captures:
  - S+0x9B8 accessor (0x14086F608): vtable 0x142608810 (image .rdata); slot1 = 0x14086FC50 and slot2 = 0x14044E0C0 are both `lea rax,[rip+X]; ret`. Write nothing. Flag set, so the lazy path is skipped at entry.
  - S+0x530 accessor (0x140872570): object 0, flag 0 in captures. Its vtable call is not taken on these captures.
  - Still unresolved: `call [r10+0x88]` in 0x14086F608 and 0x140872570/0x140872C10 (object from 0x141B55830 of an xchg on [S+0x10]/[S+0x18]). These are the remaining candidates.
- So the clearing writer is OPEN. It is not in the direct closure and not in the two resolved accessor vtables.

## 5. GF side: NOT started
- No kind-8 producer change, no red test, no build, no ALiveBall/PesStallQuick measurement. Nothing to report there.

## 6. Next steps
1. Settle the gate-on contradiction first: one unpredicated sync probe (or read the watcher log timestamp for seq13099) that reads S+0x294 in the same pause as entry. Until then the holder proof has no gate-on case.
2. Resolve the `[r10+0x88]` callee on a capture (read S+0x10 and the xchg object at 0x141B55830 path) to find the bit-4 clear.
3. GF side (kind-8 producer, ring test) after the owner chain is proven through 0x140870A40.

## 7. Disk and scratch
- Dati III scratch cleaned (empty). Proof work dirs under <workdrive>/gf-Kind8Lane-scratch (w4 456 KB). No captures moved or archived.
- <workdrive> now 282 GiB free.

## 8. Verification
- gen --check (committed tree): pass. check_pii: pass. Push: origin/Kind8Lane == 87d5588b. No test suite run (not in scope for this change).
