# PressLane8: AI+0x1414 (presser base) writer, designated owner, ActionPress wiring. No landing, no code change

Branch `PressLane4`, worktree `<workdrive>/gf-PressLane4`, tip `f95743c1` (Integrate `c4404052` merged). **Nothing committed, nothing pushed, no code changed, no build, no test run.** Scenario claimed: **none**. The anchor writer is still not found, so ActionPress cannot be ported under M1 or M2, and the captures give no PES evidence of the designated owner's resting-ball behaviour.

## 0. Verdicts

| # | Task | Verdict |
|---|---|---|
| 1 | Caller chain, rdx's record, its +0x1414 value (anch captures) | Partly CONFIRMED: rdx constant, the src record is a constant template with a zero anchor. **The hook predicate `d(rdx+0x1414)!=0` is false on the captured memory** (§1.1). Caller chain not resolved (§1.3). |
| 1b | Trace back to the staging writer, decode it | **OPEN, narrowed.** The "staging" record is not per-frame: it is a constant template (identical over 4 captures). The live anchor's writer is a computed-base store; the best lead is FUN_141138950 (§2.1), role not decoded. |
| 2 | Port the anchor, re-apply `actionpress_wip.patch`, measure, land under M1/M2 | **Not done.** No PES-proven writer to port. Numbers of record unchanged (PressLane6 §0: ALoose GREEN, PesStallQuick 6,623). Not re-measured: the tree is unchanged, so a re-run reproduces the same state. |
| 3 | ADesignated under M3 with PES evidence (arr captures + decision) | **Not decided.** The arr captures carry no MA pointer and no designated predicate. M3 not met; the test is unchanged (red). |

## 1. Capture facts (VA-cited; data read by script)

Capture set: `<workdrive>/gf-Rendezvous-scratch/live2/pid2373064/seq3808..seq3819` (hook `anch_140431800`, 12 calls, consecutive `calls` 3697..3708). Read-only: `mem.bin.zst` streamed through `zstd -dc` by `<workdrive>/gf-PressLane4-scratch/anch/readcap.py`. Nothing unpacked, nothing written under live2. Frame layout from `tools/dev/rendezvous/rv.c` (rax at 0x28 ... r15 at 0x90, xmm[i] at 0xA0+16i; xmm6 at 0x100).

- **rdx (src) = `0x0AB9FD40` in all 12.** So src = O+0x15C8 with O = `0x0AB9E778`; the staging anchor is O+0x29DC = `0x0ABA1154`. CONFIRMED.
- **rcx (dst) differs per call** (12 distinct heap objects). The copy is a whole-record copy-assign into 12 objects.
- **The src record is identical across calls.** Bytes `rdx..rdx+0x1800` are equal in seq3808/3809/3810/3811 (0 differing dwords). So the source is a constant template, not a per-frame staging record. CONFIRMED (4 captures compared).
- **Src anchor (`rdx+0x1414`, 12 bytes) = `00000000 403B0000 00000000`** in all 12 (dwords 0x0, 0x3B40 denormal, 0x0). Zero.
- **Dst pre-copy values at `rcx+0x1414`** are mixed heap data (seq3808 `B149EF3E FFF6D73E 00000000`; seq3810 all zero; seq3814 pointer-like). So the destinations are not all live AI records with a valid anchor. The copy is most likely the copy-assign used when a record is built from the zeroed template (consistent with the ctor `FUN_1421ADC70` zeroing +0x1414, PressLane6 §3). INF.

### 1.1 Predicate mismatch (DISCREPANCY for Main)

The hook was armed with `d(rdx+0x1414)!=0`. In all 12 snapshots the dword at `rdx+0x1414` is `0x00000000`. Either the watcher evaluated the predicate at a different instant (another thread writing the template, or a stale read), or its base differs. **Main to re-check the predicate against this capture before using these 12 as evidence of a non-zero presser base.** As they stand they show a zero template, not a publish of a presser base.

### 1.2 The copier (CONFIRMED, asm + decompile)

`FUN_140431800` decompiles to `*param_1 = *param_2`, then a field-by-field copy of the whole record (dec.sh run, bg log). The +0x1414 part (capstone): `0x140431B0F lea rcx,[rdi+0x1414]; 0x140431B16 lea rdx,[rbx+0x1414]; ... mov [rdx],eax; [rdx+4]; [rdx+8]`, then `+0x1420` the same. Its only E9 caller is the thunk jmp `0x1404335CB`. The thunk `0x1404335C0` has **no** E8/E9 caller (`callers.py`): it is reached through a pointer or a vtable. Matches PressLane6 §3.

### 1.3 Caller chain (NOT resolved)

call.bin `callsp = 0x22AFFDA8`, so entry = callsp+16 = `0x22AFFDB8`, but `[0x22AFFDB8] = 0`. The trampoline layout for this hook is therefore not confirmed, and the return address is not at the expected slot. Code-looking qwords on the stack: `0x141B6EFB2`, `0x141B6E343`, `0x141B3D750`, `0x141B55FD9`, `0x141B3F9CB`, `0x1403FD80E`, `0x141B3779C`, `0x141B376C0`. Their call sites (capstone, bytes before the return address):

- `0x141B6EFB2`: `FF D2` = `call rdx` (indirect).
- `0x141B55FD9`: `FF 50 10` = `call [rax+0x10]` (vtable slot 2).
- `0x141B3F9CB`: `FF 50 28` = `call [rax+0x28]` (vtable slot 5).
- `0x141B3779C`: `FF 52 10` = `call [rdx+0x10]` with `rcx = rsi` (vtable slot 2 on the object in rsi).
- `0x141B6E343`, `0x141B3D750`, `0x1403FD80E`: direct `E8`.

So the copier is reached through vtable slot 2 of an object. Ghidra decompiles those return sites only as fragments (no `.pdata`; `dec_chain.txt`), so their owners are not resolved. INF.

## 2. Live anchor writer search (CONFIRMED scan; roles INF)

`<workdrive>/gf-PressLane4-scratch/anch/scan_disp.py` = capstone linear sweep of `.trace` (0x140001000, 0x252D800 bytes, resync on invalid bytes), printing every instruction with a memory operand at disp 0x1414, 0x1418 or 0x141C. Output `scan_1414.txt`: 110 hits. Real stores (`mov` to memory; `cmp`/`inc` excluded):

| VA | Instruction | Note |
|---|---|---|
| `0x14113909B`, `0x1411390A9` | `mov dword [rbp+rcx*8+0x1414],eax` / `+0x1410` | **lead** (§2.1) |
| `0x14113989A`, `0x1411398A9` | same, second path | **lead** (§2.1) |
| `0x141E5FB20` (+0x1418, +0x141C) | unrolled block fill `[rcx+rdx+0x1414..]` | template fill loop, not per-frame (§2.2) |
| `0x1421ADD16` | `mov [rbx+0x1414], rsi` | the ctor zeroing (PressLane6 §3) |
| `0x1408AEAF9`, `0x1408AEB02`, `0x140A57FCC`, `0x140A58A25`, `0x140ED3908`, `0x140F1D0A7`, `0x141183628`, `0x141184842`, `0x1411EB941`, `0x1421728FA` | stores at +0x1418 / +0x141C | other records (not AI+0x1414); not studied |
| `0x1422A3F16`, `0x1422A41D6`, `0x1422A5666`, `0x1422A5EA7` | `inc`/`cmp`/`mov` at +0x1418 | other record (counter); not studied |

No direct `movss [reg+0x1414]` (or `+0x1418`/`+0x141C`) store with a plain base exists in `.trace`. Every live writer found uses a computed base (indexed or a copy loop).

### 2.1 Lead: FUN_141138950 (`.pdata` 0x141138950..0x141139B53), CONFIRMED asm, role INF

- Single E8 caller: `0x141138563` (FUN_141138563, decompile in `dec_fn.txt`).
- `0x141139094 mov eax,[rbp+rdx+0x1924]`; `0x14113909B mov [rbp+rcx*8+0x1414],eax`; `0x1411390A2 mov eax,[rbp+rdx+0x1920]`; `0x1411390A9 mov [rbp+rcx*8+0x1410],eax`, with `rdx = (r13+rsi)*0x3C` and `rcx = rbx*9` (`lea rcx,[rbx+rbx*8]`, then `*8`: a 0x48 stride). The second path (`0x14113989A`) uses the same pattern with `rcx = (rdx*5+rax)*8`.
- So a per-entry 0x3C-stride table at `rbp+0x1920/0x1924` is copied into a 0x48-stride record array at `+0x1410/+0x1414`. The loop runs 18 entries (`cmp r10d,0x12`). The function builds stack-local 0x3C tables (`auStack_1c50 + lVar11*0x3c`) and calls `FUN_1414bb580`, `FUN_1414bb2a0`, `FUN_1414baec0`, `FUN_1414e2480`. It reads like a candidate-list builder. The AI record stride is 0x28 (PressLane6 §3), so the 0x48 array is not yet identified as the AI record.
- **Open:** whether the 0x48-stride array is the presser's base record (the one `FUN_140A489F0` returns), and what writes the 0x3C table at +0x1920/+0x1924.

### 2.2 `FUN_141E5FB20` (`.pdata` 0x141E5FAB0..0x141E5FD93, CONFIRMED decompile + asm)

Unrolled loop: `*(param_1+param_2+0x1414) = in_EAX; ... = param_1[0xb..]` for the whole record. It fills an array of records with stride `param_2` from a template at `param_1`. A template fill, not a per-frame publish. Excluded as the presser writer.

## 3. ADesignated (task 3): no decision

- The arr captures (`arr_1409F34A0`, 12 in `live2/pid2373064`: seq18496, 18542, 18749, 18864, 19439, 19485, 19646, 19692, 19899, 20014, 3830, 7445) have **xmm6 = 0.0 at entry in all 12** (the killed-theory check: PES's threshold is 0, GF matches). CONFIRMED.
- Their GPR slots do not hold the MA pointer: `r15` = `0xC1256C9D`, `0xC1B6DCE2`, `0x3D4CCCCD`, ... (float bit patterns, not a heap pointer). So `r15+0x2600C` / `r15+0x2601C` cannot be read from these frames, and **the designated arm cannot be told apart in them.** The predicate in the brief (`d(r15+0x2600c)==d(r6+4)`) was not set on these 12. Not usable as designated-owner evidence.
- So M3 is not met: no PES cite of the designated owner's target on a resting ball. ADesignated stays as it is (red).

## 4. Task 2 status

- `actionpress_wip.patch` is **not re-applied**. Its wiring needs the presser base (AI+0x1414) to be PES's, and that base's writer is not ported. Any re-application would measure the GF stand-in, i.e. the PressLane6 state. Numbers of record (unchanged): ALoose GREEN, PesStallQuick 6,623 (bound 53, M2 cap 106); loose-presser-only 9,130. Both fail the bounds. Nothing lands.
- Ratchet on the tip: unchanged (no code change). Last measured on Integrate: PesStallQuick 53, ALiveBall 2229 (tasks/23-11-26.md).

## 5. Rig requests (VA + predicate + what the capture must show; Main generates the lines)

1. **`FUN_141138950` entry, `0x141138950`**, FIRST 12 EVERY 20, cap 12, FULL. Must show: rbp (the base for +0x1920/+0x1924 and +0x1410/+0x1414), r13 and rsi (the loop index), rbx (the stride index, `[rsp+0x4c]` at `0x14113908C`), the 18 entries of the 0x3C table at `rbp+0x1920`, and the return address `ret` (expected `0x141138563`). Why: it is the only computed-base store into a +0x1414 pair found in `.trace`.
2. **Re-check the copier predicate.** `anch_140431800` with `d(rdx+0x1414)!=0` is false on the 12 captures' own memory (§1.1). Ask Main to re-verify the expression. Needed only to tell a zero template from a live presser base; not needed for request 1.
3. **Copier thunk owner** (deferred until request 1 names the owner): `call [rdx+0x10]` at `0x141B3779C` / `call [rax+0x10]` at `0x141B55FD9`, with `ret` recorded.
4. **ADesignated arm** (only if a MA pointer is in the frame): `0x1409F34A0` entry with predicate `q(r15+0x2600c)` readable and `d(r15+0x2600c)==d(r6+4) and d(r15+0x2601c)==0xff`. Current arr hooks have no MA in r15.

## 6. Open items and next exact steps

1. (Main) Re-check the copier predicate (§1.1). Until then the 12 captures show a zero template, not a publish.
2. (Static, after request 1) Decode what writes the 0x3C table at `rbp+0x1920/0x1924`, and whether the 0x48-stride array at `rbp+0x1410` is the presser AI base read by `FUN_140A489F0`.
3. (Static) Identify the vtable owner of the copier thunk `0x1404335C0`, from a `.rdata` slot read on a capture (flat image slots are plain pointers, per the 17-11-26 correction).
4. Only after (2) names a PES writer of AI+0x1414: port it (red-first), wire `actionpress_wip.patch` for PRESS men, re-measure ALoose / ADesignated / PesStallQuick / ALiveBall (M1, or M2 with the named stand-in).
5. ADesignated: decide only under M3, after (2)–(4) show PES's designated-owner target.

## 7. Blockers

- Predicate mismatch (§1.1): Main.
- The writer's record identity (§2.1): rig request 1, or a decode of its caller `0x141138563`.
- The arr frames carry no MA (§3): designated-arm evidence needs the MA pointer in a hook's frame (request 4).

## 8. Artifacts (scratch only; nothing in live2 touched, nothing deleted from live2)

- `<workdrive>/gf-PressLane4-scratch/anch/readcap.py`: reads VA ranges from a packed or full capture by streaming `zstd -dc` (no unpack).
- `<workdrive>/gf-PressLane4-scratch/anch/scan_disp.py`: capstone scan of `.trace` for disp operands; output `scan_1414.txt` (110 hits).
- `<workdrive>/gf-PressLane4-scratch/anch/src_3808..3811.txt` (src record dumps), `dec_chain.txt` (return-site fragments), `dec_w.txt` (FUN_141E5FB20), `dec_fn.txt` (FUN_141138950, FUN_141138563).
- My `/tmp` files were removed.

## 9. Summary

- Scenario: **none.** Nothing lands. Tip `f95743c1` unchanged; no push.
- CONFIRMED: rdx constant (0x0AB9FD40); src record a constant template with a zero anchor (4-capture compare); the copier is a whole-record copy-assign; the arr captures have xmm6 = 0; no plain-base store to +0x1414/+0x1418/+0x141C in `.trace`; FUN_141138950 is the first computed-base store into a +0x1414 pair from a 0x3C table (single caller `0x141138563`).
- INF: the anch captures are template-based object construction, not a per-frame publish; FUN_141138950 is on the presser-base path (unconfirmed).
- OPEN: the live writer's identity; the copier thunk's caller chain; the designated owner's resting-ball target; the predicate mismatch.
