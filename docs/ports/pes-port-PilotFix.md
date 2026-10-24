# pes-port-PilotFix: the pilot translator after Main's rulings (branch PilotFix, head b443232)

## Result

- Full run (252 attempted): **216 diffed**, 36 undiffed (33 live state, 3 struct map gap), 0 translator-bug failures left.
- Against the committed 191 record and against main's recorded 173: **no function lost**. 43 gained over main's 173, 25 over the 191 record.
- Every function that left the record: **none**. The 8 functions lost in ee02a93 are bit-exact again (14042D470, 1404C8DC0, 140A40820, 140A42940, 140A8FF10, 14149A470, 141E532D0, 141E532E0).
- Open: the second step of ruling 1 (dump-time `.data` values are still read by 69 diffed functions, so they stay provisional). See "Open".

## Counts per step (each commit: driver --check green before commit)

| step | commit | count | what it showed |
|---|---|---|---|
| 0 | ee02a93 (Main's commit of my writable fix) | 191 full run | 8 lost vs 173 |
| 5. tail-stop counts only in-window targets | 0621a22 | check 191/191; 140A40820, 140A42940 bit-exact (targeted) | CONFIRMED cause: decode ran past a far Jcc target into garbage |
| 4. run.py streams the image in 16 MiB chunks | 0621a22 | runs under `ulimit -v 2000000` (cap unchanged) | |
| 1. diff main maps the image as run.py maps a flat dump | bea32fc | check 191/191; targeted 12/13 | all 8 lost back; 1404BCAD0, 140A92C80, 140A533A0, 140A6E150 also pass |
| MOVZX/MOVSX load the source width | 5b8d20c | check 191/191; 140A6E1E0 bit-exact | CONFIRMED: `MOVZX eax, byte ptr` read four bytes; the zero table had hidden it |
| 1b. .data globals as case-varied inputs (enc absolute) | 95f986f | check 191/191; targeted 14/14 | 141E532D0, 141E532E0, 14149A470 and callers 14042D470, 1404C8DC0 no longer provisional |
| 3a. spec outputs audit (spec_outputs.py) | 5484011 | full 216 diffed; no loss vs 191 or 173 | 84 specs now check xmm0/eax/rax they write (ret_xmm0, ret_eax, ret_rax) |
| 3b. register-indexed rdata tables baked | f8d3236 | check 216/216; full 216 | 92C80 and the 6E group are baked (ro_rd windows) |
| final full run | b443232 | 216 diffed, 0 lost | results.json committed from this run |

## Rule 2 (false passes)

No function left the record. One earlier pass was shown false and then corrected rather than dropped: 140A6E1E0. Its zero-table read hid a MOVZX width bug; after the image mapping it failed (25 mismatches on ret_eax), and after the MOVZX fix it is bit-exact. The 84 augmented outputs passed for every recorded function, so no earlier pass was passing only by leaving its return register unchecked.

## Bake (3b): what is and is not baked

- Baked: 92C80 (table 0x142655950, guard CMP 0x1FF then JA) and the 6E group (table 0x1426484F0, guard CMP 0x5E then JL, MOVZX byte reads). CONFIRMED: the generated TUs contain `ro_rd(k_t..)` windows and the functions are bit-exact.
- Not baked: 140A40820, 140A42940, 1408CB0B0, 140A603C0. They read rdata through a register index with no straight-line LEA of the base, or no guard. They are bit-exact against the mapped image, but the shipped lib (PesState with no image) would read zero there. OPEN.
- INF: the shipped LaneZ lib has not been built in this run. The translator output changed (baked windows, MOVZX fix), so the cutover build and offball suite must be run by the gate.

## Open

1. **Ruling 1, second step.** 69 diffed functions remain `provisional`: they read dump-time `.data` values at absolute VAs that are not patch_addrs and not inputs. Some of those reads are pointer-typed (IMG chase), so a random scalar input would be wrong; they need region modelling, not inputs. Not enumerated per VA in this run.
2. **33 live-state functions** (IMG reads). run.py faults while generating their corpus (unmodelled reads). Same family as item 1.
3. **ImageDump's 10 varying IMG fields.** No translated TU reads any of the 10 VAs as an absolute literal (grep over the 252 TUs: 0 hits). Their values enter through the spec's synthetic IMG regions (case inputs), so they are not dump-read by the diff. Not verified further.
4. **140A8FF10 precheck is stale.** Its TU reads no writable absolute VA; the provisional note comes from `live_data_reads: ["IMG"]`. To correct in the next spec pass.
5. **Harness register state (INF).** run.py does not reset GPRs or XMMs between cases; the diff main starts each case at zero. A leak would show as a mismatch, not a false pass. None appeared with the 84 new outputs.

## Hygiene

- All runs held `ulimit -v 2000000`. Order deviation: step 4 (streaming) was done before step 1, because step 1 tests cannot run without it.
- No /tmp use in this turn. Scratch is in <workdrive>/gf-PilotFix-scratch.
- Gate not run (Main gates). No pushes except PilotFix; origin/PilotFix = b443232 (ls-remote confirmed).
