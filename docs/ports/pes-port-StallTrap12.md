# pes-port-StallTrap12: PES's ball-state word B+0x291C is a copied snapshot, not a GF-shaped flag; the kind-8 producer FUN_140A179C0 selects its builder by B+0x291C/B+0x2918/B+0x2920; PES's captures show state (1, 0, 1), which routes to the decision FUN_140A19290; hook lines for the rig are ready (CounterPath, 22-11-26)

Status: NOT LANDED. Nothing committed or pushed; origin/StallTrap is 56ef1e91. The feed, race tests and the four match tests were not run: no kind-8 producer is translated and proven, so there is nothing PES-shaped to feed. Correction of pes-port-StallTrap11 is in section 2.

## 1. Static decode (CONFIRMED unless marked)

Scan of the whole code section for memory operands with displacement 0x291C (capstone, 10,513,288 instructions, `scratch/trace/scan291c.py`, output `scan291c.txt`). 14 hits:

| VA | access | instruction | enclosing function (.pdata) |
|---|---|---|---|
| 0x140430D4A | W (8 B) | `movsd [rbx+0x291c], xmm1` | chunk of FUN_140430C30 (0x140430C30..0x140430E19): whole-ball-state copy from param_2 to param_1 (copies +0x2798..+0x2968) |
| 0x14087351D | W (8 B) | `movsd [rsi+0x291c], xmm1` | chunk of FUN_140872C10 (0x140872C10..0x1408735DB): ball sync, copies the source record's fields into lVar12 = `*(G+0x40)`-derived object |
| 0x140A5790C | W (8 B) | `mov [rcx+0x291c], rax` | no .pdata entry; Ghidra function FUN_140a5790c (body 0x140A5790C..0x140A57923): `B.291c = rax (8 B); B.2914 = al; B.290c = 0xFF` (a reset) |
| 0x14050226A | R | `cmp [r14+0x291c],3` | - |
| 0x1407403DC | R | `cmp [r13+0x291c],2` | - |
| 0x14074324C | R | `cmp [r15+0x291c],2` | - |
| 0x14087B8A5 | R | `cmp [rsi+0x291c],1` | - |
| 0x140A179F6 | R | `cmp [rbp+0x291c],1` | FUN_140A179C0 (the kind-8 producer's gate) |
| 0x140A17B61, 0x140A17B87 | R | `cmp [r15+0x291c],1/2` | just after FUN_140A179C0's body (0x140A17B2C); not in its .pdata extent |
| 0x140A188AE | R | `cmp [rsi+0x291c],1` | FUN_140A18810 (in-play builder) |
| 0x141F4D127, 0x141F4D162 | R (1 B) | `cmp byte [r9+rcx+0x291c],0` | indexed, not the ball field (ignore) |

Writers are copies: the ball-state word is set by copying a source record (FUN_140430C30's param_2, FUN_140872C10's param_1), and reset to 0xFF holder by FUN_140A5790C. The copies are reached from: FUN_140430C30 by `jmp` at 0x1404335BB (a function near 0x140433xxx); FUN_140872C10 by `call` at 0x140425D97. FUN_140A5790C has no direct caller (reached indirectly). Not decoded: the sources of the copies (what param_2 / param_1 hold in play) and the caller of FUN_140A5790C. OPEN.

### FUN_140A179C0: the kind-8 producer (decompile, CONFIRMED)

```
lVar2 = *(_DAT_1436f3dc8 + 0x40)          // B: the ball
lVar3 = *(_DAT_1436f3dc8 + 0x270)         // MI
if (B.291c == 1 && gate(B) != 0)          // gate = bit 4 of B.2918 (func 0x140A57950)
    ok = FUN_140A18810(param_1, &actor, param_3)   // in-play builder
else
    ok = FUN_140A19060(param_1, &actor, param_3)   // default builder, selector B.2920
piVar6 = event query(param_3[1] = E, 8)   // newest kind-8 record (0x37 = none)
if (*piVar6 != 0x37 && piVar6[4] == MI.1168 && FUN_140A60300(piVar6+7, key) != 0) return
if (ok) { payload = FUN_140A867A0(); key = FUN_140A62FE0(*param_3 = team, actor);
          FUN_140A867D0(payload, ...); FUN_140A3D060(param_2, 8, MI.1198, MI.1198, FUN_140A3C050(), MI.1168);
          copy payload into param_2 }
return ok
```

- The record is pushed by its caller (the router FUN_140A3DC40 chooses ring 1 for kind 8).
- `FUN_140A62FE0(team, slot)` returns the key `{team, slot}` when slot < 0x16 and the roster entry is valid, else `{2, 0xFF}`. That is PES's none key, the value in the ring-1 record below.

### FUN_140A19060 (default builder) selector B.2920 (CONFIRMED)

- 1 -> FUN_140A19290 (the decision; not fully decoded, see section 4).
- 2 -> if FUN_140A57940() and MI.173C < 0x16: actor = MI.173C, ok = 1.
- 4 -> if `*(*param_3+0x108)[1] != 0`: actor = `**(*param_3+0x108)`, ok = 1 (the holder pair).
- otherwise: actor = 0xFF, ok = 0.

FUN_140A18810 (in-play builder), for B.291c == 1 with the gate on: actor from B.290C (last roster index) under a switch on B.27DC; returns ok = 1 except case 0x10. Not used in the captures (gate bit 0).

## 2. Corrections to round 11 (pes-port-StallTrap11)

- "ballState 4 = holder pair" was wrong. The selector is B.2920 (state 4 is B.2920 == 4); B.291c == 1 is the in-play gate, and the in-play path is FUN_140A18810.
- The GF "kind-8 never routed" finding stands. What GF lacks is the producer FUN_140A179C0 and its input state, not a ball-state word of one value.

## 3. What PES's captures hold (CONFIRMED, read from mem.bin of the 12 writer captures, `scratch/trace/readcap.py`)

Same in all 12 captures (seq392, 394, 395, 397, 401 shown; the others the same):
- B (ball) = 0xABE8B04. B.290C = 0xA (roster 10), B.2914 = 6, B.2918 = 0, **B.291C = 1**, **B.2920 = 1**, B.27DC = 1.
- So the gate is 0 (bit 4 of 0x2918 clear), the selector is 1, and the builder is FUN_140A19060 -> FUN_140A19290.
- MI.116C = 1 (play), MI.173C = 0xA, MI.1168 = 1, MI.1198 (frame) = 0xA8..0xB8 (168..184 across the 12).
- Ring 1 (E = 0x7FF5FDAE0018, the writer's event store): count 2, head 2.
  - newest kind 8: frame 167, key `0xFF00000002` (team 2, slot 0xFF = none);
  - previous kind 8: frame 43, key `0x0A00000000` (team 0, slot 10 = B.290C).
- Newest kind-7 (touch) frame stays 167 (round 10).

So at the writer call PES's newest holder record is "none" (team 2 / 0xFF), and the holder before that is roster 10 (the ball's B.290C). That is the value GF's ring must reproduce.

## 4. Open: the decision FUN_140A19290

It sets the actor for selector 1: it reads MI.116C (phase; the `== 1` branch is play, then `== 2`, `== 3` branches), MI.11C5, MI.1740/173C, MI.1480, MI.1554, MI.1574, the ball speed (FUN_140A57720 of B, length compared with 0x14259CE38 * ...), the men list (FUN_140A48A10 / 48A30), and a per-roster check FUN_140A48910 with fields +0x1c, +0x1a, +0x1 (team/kind). Its output is the actor (or 0xFF) with a status. It is ~400 lines of decompile; not finished this round. OPEN: which MI fields are constant across the 12 captures and which vary.

## 5. Hook lines for the rig (window-checked: `untranslated_callees.hooks_for_image`, refused: none)

```
hook kind8_140430C30 0x140430c30 16 1 1 48895c2408574883ec20488bfa488bd9
hook kind8_140872C10 0x140872c10 14 1 1 4c8bdc55535741564157498d6b98
hook kind8_140A179C0 0x140a179c0 16 1 1 48895c2408555657415541564883ec50
hook kind8_140A19290 0x140a19290 15 1 1 488bc4488958084c89401848895010
hook kind8_140A3D060 0x140a3d060 15 1 1 40534883ec20f30f10442450488bd9
hook kind8_140A3DC40 0x140a3dc40 20 1 1 405556574881ec8006000048c7442428feffffff
```

Suggested predicates (the watcher's `--when`):
- `kind8_140A179C0`: `b(q(q(0x1436F3DC8)+0x40)+0x291c)==1` (in play), cap 12.
- `kind8_140A19290`: any call, cap 24 (the decision, all phases; captures to the MI phase).
- `kind8_140A3D060`: `(rdx&0xffffffff)==8` (kind-8 records only), cap 12.
- `kind8_140A3DC40`: `d(rdx)==8` (the record's kind is 8: ring 1), cap 12.
- `kind8_140430C30`, `kind8_140872C10`: cap 6 each (the ball copies; their source is open).
Full captures, never slimmed: these are the kind-8 proof inputs.

## 6. Plan for GF (not done; blocked on captures)

1. Hook as above; capture the decision (FUN_140A19290) and the producer (FUN_140A179C0) calls. Each capture gives the B/MI/E state PES actually used.
2. Translate FUN_140A179C0 with its closure (xlate.translate_one, pilot = the closure's set), prove it on the captures (snapdiff), then feed it from GF's state: B.291c and B.2920 from GF's ball (sim state), the holder from GF's roster (B.290c), MI from GF's MI. Red-first test: a kind-8 record exists in ring 1 after a touch in play.
3. Translate FUN_140A19290 (the selector-1 decision) the same way. Its inputs (phase, ball speed, roster classes) must come from GF's world, not stand-ins.
4. Re-run the eight-window lockstep (GF_CB_TRACE) to the next divergence, then the feed (RunCounterB), the race test, PesStallQuick, full ALiveBall, PesHang, PesPassTarget.

## 7. The six stand-in closure bodies (answer to Main's question)

| file (src/pes/translated/closure/) | introduced by | stand-in | gen --check |
|---|---|---|---|
| pes_fn_140676F40.cpp | aaac3218 (writer 0x14092ED30 translated, 12/12) | `pes_model_sqrtf` (libm sqrt model) | not skipped: no "hand-refined" in the first three lines |
| pes_fn_14078E470.cpp | aaac3218 | `/* stub: engine-noop */` (engine no-op, dropped) | not skipped |
| pes_fn_140677580.cpp | aaac3218 | sqrt model | not skipped |
| pes_fn_140706350.cpp | aaac3218 | sqrt model | not skipped |
| pes_fn_140A581C0.cpp | fc84d383 (trap chain, kind-0 root 0x14079A470 and commit 0x14079AFD0) | stand-in (not reproduced by my regeneration) | not skipped |
| pes_fn_140A58550.cpp | fc84d383 | stand-in | not skipped |

- gen_translated.py `check_layer` (line 339-350) skips a file only when "hand-refined" appears in its first three lines. None of the six has it, so `gen --check` regenerates and compares them. Whether the committed bodies pass gen --check: NOT RUN this round (a full regeneration takes about 25 minutes warm). My own regeneration with the replay stub map (`ft.stubs_for_run`) reproduced the writer exactly but not these six (they differ at the stand-in lines), so the committed stand-ins come from the driver's per-record stubs, not from the replay stub map. The check is therefore the test of record; run it before the next landing.
- Owner-rule status: a stand-in is an approximation and needs its `// ponytail:` naming the PES site that removes it. None of the six carries one.

## 8. Not done (honest list)

1. No translated kind-8 producer or decision; no proof; no feed; no race tests; no PesStallQuick, ALiveBall, PesHang, PesPassTarget; no gen --check; no fast-tier gate. Nothing pushed.
2. FUN_140A19290 not decoded fully (section 4). FUN_140A5790C's caller not found. The source records of the B+0x291C copies are open.
3. Captures are the only source of B/MI/E values; they are the same across 12 captures (one match window), so they do not yet show how B.2920 or B.291C change.

## 9. Artefacts (scratch, not committed)

- `scratch/trace/`: scan291c.py / scan291c.txt (the displacement scan), calls.py / callers_291c.txt, callers2.txt (callers of the kind-8 chain), dec_179c0.txt, dec_builders.txt, dec_containers.txt, dec_19290.txt (decompiles), readcap.py (capture reader), hooks (printed above).
- Repo: <workdrive>/gf-StallTrap at 56ef1e91, src/tests clean, build/ removed.
