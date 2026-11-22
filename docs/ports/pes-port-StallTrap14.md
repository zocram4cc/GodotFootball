# pes-port-StallTrap14: kind-8 chain (record builder, decision, producer, in-play key builder) proven and LANDED as the kind8 layer (4bcf2b94, pushed); the B+0x2918 bit-4 source is the ball setter FUN_140A571F0 (CounterPath, 22-11-26)

Status: LANDED (translated and proven, gen --check green), pushed to StallTrap at 4bcf2b94 (ls-remote confirmed). NOT done: the GF side. No GF call site, no feed, no red-first test, no lockstep, no race or match tests. Items 2 and 3 of the brief are open; the blocker is a hook for the bit-4 source (section 3).

## 1. Landing (CONFIRMED)

- Commit 4bcf2b94 on origin/StallTrap (56ef1e91..4bcf2b94). 100 files: src/pes/translated/kind8/ (4 roots) plus 130 closure members in src/pes/translated (committed as closure code, not proven), results_trapaction.json, closure_kind8_*.json, specs/trapaction, universe.json, closure-kind8.txt.
- gen_translated.py --check: "10223 functions (3829 recorded bit-exact, 6394 closure) regenerate byte-identical", CHECK_RC=0.
- Commit made with `git -c user.name=... commit` and explicit paths. The eval `commit` namespace reported "Selected: nothing" because it works on the main checkout (efd50d91c7 on master), not this worktree, so I did not use it.
- PII grep on the staged diff: no <workdrive>/ or <home> Not run: tools/check_pii.sh (it fails in the worktree because of the symlink; AGENTS says run it from the main checkout). OPEN: Main should run it there before merge.
- The gen regen changed one existing file: pes_fn_140440FC0.cpp now calls 0x1403FB5B0 (translated) instead of a dispatch-missing stub. That is the correct consequence of the new closure member.

## 2. Proofs (CONFIRMED by prove_bounded.py output; all PES bytes under unicorn, the proof is of the translation vs PES)

| root | proof input | calls | bit-exact | own-block | note |
|---|---|---|---|---|---|
| 0x140A3D060 (kind-8 record builder) | 7 real kind-8 calls: seq344, 524, 660, 740, 1067, 1117, 1310 | 4 | 4/4 | 5/5 | seq1117 and seq1310 are not replayable (replay reads address 0x8); excluded. Real captures are of the root itself. |
| 0x140A19290 (decision) | 24 real calls, seq384..416 | 24 | 24/24 | 21/259 | every call is a keep-holder (false) exit. The 12 loose-ball exits (true, actor 0xFF) are NOT covered. |
| 0x140A179C0 (producer) | 11 real calls: seq594, 694, 787, 875, 957, 1033, 1104, 1181, 1253, 1335, 1439 | 11 | 11/11 | 14/23 | all with B+0x2918 bit 4 clear (gate off). The in-play gate-on branch is not covered. |
| 0x140A18810 (in-play key builder) | 3 SYNTHETIC states (synth.py, rdx = scratch out) over seq1067, 1117, 1310 memory (B+0x2918 = 0x12) | 3 | 3/3 | 9/19 | no real call of 18810 was captured; the states are synthetic over real memory. |
| 0x140A62FE0 (key builder) | already proven (results.json: 58 cases, own-block 1.00) and already translated (ai/) | - | - | - | not re-proven |

Replay evidence (not the proof): the in-play builder's actor, passed through 0x140A62FE0, gives the live ring-1 keys 0x200000000, 0xFF00000002 and 0x400000000 at the three gate-on records (seq1068, 1118, 1311). This is the strongest evidence that the in-play path matches live.

## 3. Writer of the bit 4 that selects the in-play path (CONFIRMED structure, value OPEN)

- The producer chooses the in-play builder on B+0x2918 bit 4 (`FUN_140A57950`).
- Writers of B+0x2918 (scan of the whole code section for displacement 0x2918): 0x140A57261 (`mov [rbx+0x2918], rdi`, qword, inside FUN_140A571F0, the ball reset/set: also writes B+0x2920 = low dword of rdi, B+0x290C = 0xFF, B+0x2928 = 1, B+0x292C, B+0x2930 = 1). So the bit-4 value is rdi at that store.
- FUN_140A571F0 is called only from FUN_14046DC30 (direct E8 scan, `callers_571f0.txt`: 0x14046DC6D). FUN_14046DC30 is inside FUN_14046DC00 (not an entry). Its rdi at the set path is the value that matters, and it is inherited from the caller of 14046DC00 (not traced).
- The copy into the snapshot: FUN_140430C30 copies B+0x2914..0x291B from a sibling record rdx = B+0x296C; that snapshot is written by FUN_1404335B0 (`*(param_1+0x296C) = *(param_1+0x52D8)`). And B+0x5284 (the snapshot's 2918 word) is reset by FUN_141F4CB6F (no bit 4 there). So the chain is: setter 571F0 (rdi) → B+0x2918 → (snapshot) → 430C30 copy → B+0x2918 at tick start. Which of these copies carries the bit into the captures is not decoded.
- The displacement scan found one write at +0x296C (`mov dword [r15+0x296C], 2`, FUN_14047CC74) — the state 2 of the other snapshot, not the bit.

### Hook lines for the rig (window-checked; `untranslated_callees.hooks_for_image`)

- `hook bs_14046DC00 0x14046dc00 19 1 1 4889542410574883ec3048c7442428feffffff` — the caller head. Predicate suggestion: none (or `(rdx&0x10)!=0` if the caller's rdx is the state). Cap 12, FULL.
- `0x140A571F0` (the setter) is REFUSED by the window check (a call at 0x140A571FD inside the displaced window). Use the pair: capture 14046DC00 entry (its rdi/rcx/rdx), and the setter's state from the dd of B+0x2918 at the 14046DC00 entry snapshot.
- Predicate for the rare case: the captures show B+0x2918 = 0x12 (gate on) in 6 of 47 captures; a useful predicate is `b(q(q(0x1436F3DC8)+0x40)+0x2918+0x2)` ... Not verified; Main should pick a predicate on the ball word at B+0x2918 once it knows the offset convention, or just cap 24 unfiltered.

## 4. Stand-ins (answer to the round-12 question, now checked)

- gen --check ran with the six stand-in closure bodies included and was green. The six (`140676F40, 14078E470, 140677580, 140706350, 140A581C0, 140A58550`) carry no "hand-refined" banner, so gen --check regenerates them and they match. They are closure members of the committed layer; the ponytail/stand-in comments are still missing (the owner rule). OPEN.

## 5. Not done (honest list)

1. GF side: producer in GF's event stage, feed of B+0x2918 bit 4 from GF's ball, red-first test, lockstep, race tests, PesStallQuick, full ALiveBall, PesHang, PesPassTarget. Blocked on item 3.
2. The loose-ball exits of the decision (12 exits): no capture with true. Needs a hook predicate on an in-play decision with a loose ball.
3. tools/check_pii.sh from the main checkout: not run.
4. The eval `commit` namespace is bound to the main checkout, so the landing used path-limited git.

## 6. Scratch

`scratch/trace/`: prove_d060/, prove_19290.log, prove_179C0.log, prove_18810.log, synth18810/, scan296c.txt, scan5284.txt, dec_5284.txt, dec_2918.txt, dec_setter.txt, check1.log (gen --check, CHECK_RC=0), gen1.log.
