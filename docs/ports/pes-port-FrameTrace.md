# FrameTrace: PES's match step run from open-play captures, and impure reach (26-10-26, final)

Branch `FrameTrace` (from `origin/Captures` f5a42e32). Pushed `02b03e81` (`ls-remote` confirmed). No code changed after that push. Tools: `tools/dev/translate/capture_frame.py`, `tools/dev/translate/frame_trace.py`. Scratch (captures, banks, logs) is PES-derived and not committed.

## Result

- **Reach: 1 of 50.** Only the team tick `1404F6C30` is entered (once per frame). The other 49 are never entered.
- **Frame loop: not reproducible.** Same capture, same arguments, different outcomes: 41 frames (one early run), 6 frames (`--cap 4`), 1 frame (`--cap 32`, repeated), and 3 frames clean for six hash seeds. The stop is always the same unmapped read, `0x363E2888` (size 8), from RIP `0x141E5CA52`, inside the tick path. The stop frame varies (0, 5, 40). Cause OPEN (§4).
- **Coverage: none.** The tick is `unmodellable` for the driver (closure reaches `1404FEDC0` → `140A58550` → import slot `14252FDE0` with no name in the image). Other 49: no calls, so no coverage.
- **Bit-exact: 0 of 1 translated, 0 of 1 diffed.** The driver ran 64 cases over the tick bank and reported `class=unmodellable`.
- **Comparison with 16/50 at the floor (median 0.30): not comparable.** This run reaches 1 function and nothing translates. The union result is far better, and this run cannot show coverage.

## 1. Captures

| capture | play / per | frame | consistency | frame delta | used |
|---|---|---|---|---|---|
| `f501-open-p3-play1-fr43009` | play 1 / per 3 | 43009 to 43011 | MI, H vtable, G slots | 2 | yes (primary) |
| `f1323-open-p1-play1-fr13724` | play 1 / per 1 | 13724 | same | 2 | yes (frame run: 1 frame, stop at 0x140A3E8E0, read `0x7FF5DFDE7208`) |
| `f1559-open-p1-play1-fr19044` | play 1 / per 1 | 19044 | same | 2 | yes (frame run: 1 frame, stop at 0x141E5CA52, read `0x2CA3A7D8`) |
| `f01-open-p3-play1-fr30490` | play 1 | 30490 frozen | delta 0 | 0 | rejected as frozen |

- The capture job was stopped by SIGTERM after ~2560 s and about 700 tries. Only 4 accepts in total.
- **Captures' 13 open-play captures** (`<workdrive>/gf-Captures-scratch/captures_*/`): none has the H object. Their `live.json` has no `h` key, and `capture_midplay` reads only the H global page, not H. Without H, the match step cannot run (`H_STEP` needs `*(0x1436F3F98)`), and the team tick's brain and `S` arguments are not captured. These captures cannot run the tick.
- Gate rejections in the open-play run: mainly `frame_delta` 6 to 9. The BFS (1500 objects) takes about 0.1 to 0.15 s, which is about 8 frames at open-play speed.

## 2. Frame run and bank

`frame_trace.py` maps the capture over `pes_full.bin` (flat image), then runs `H_STEP = 0x14041ECF0` per frame. Impure entries are hooked; each call is snapshotted as a seed artifact and reservoir-sampled into `<VA>.bank.json` (driver format).

- Tick entries: 1 per frame. `MI+0x1168` = 3 (per 3) for the `f501` run, 1 (per 1) for the other two.
- Stubs: 4 `stubs.json` models apply; unmodellable callees run their image bytes.
- Ordered trace (`diag_order.py`, one frame): the tick executes about 30 instructions, then calls `0x141E5D4C0`, and then faults on `0x363E2888`.
- Tick A2 (r8 at tick entry, from the untyped-roots row `mov r8,[rbx]` at `14041EDD7`): `0x1bc7df48`. Not G's team slot 0 (`0x1bc71e04`) or 1 (`0x1bc8ff14`). Its first 8 bytes equal team 0's (`5201000001000000`), so class team is plausible but unconfirmed (one header match). A0 (rcx, brain) `0x33c50dc0`.

## 3. Driver

`driver.py --only 1404F6C30 --seed-bank run50/banks --perturb`: `translated=False diffed=False cases=64 class=unmodellable callee 1404FEDC0`. No translated code exists for this spec, so no bit-exact or coverage result.

## 4. Open items (numbered)

1. **Frame-run nondeterminism (OPEN):** the same capture stops at different frames (0, 5, 40) across runs with identical inputs. The read address and RIP are stable. Hash-seed variation with 3 frames gave clean runs. Next: check for uninitialized unicorn state carried between runs (xmm, flags), and the Bank's `snapshot` reads inside hooks.
2. **Pointer budget:** `0x363E2888` is a heap object reached through the tick's step-1 region (`0x141E5D4C0`). It is not in the capture (1500-object BFS budget). Fix: follow from the tick's brain (`H+0x450+8s`) first, or raise the budget.
3. **Why the other 49 are not reached:** the tick leaves after about 30 instructions into `0x141E5D4C0`. Need the branch gates at the tick entry (brain flags, `T+0x8c`, H.b MI frame check) in the ordered trace.
4. **Frame gate vs BFS speed:** the gate of 2 almost never passes at open-play speed. Fix: fewer objects or a relaxed gate justified by object-level checks.
5. **Side 1:** only one tick entry per frame was seen; whether brain for side 1 is null is not verified.
6. **Typed roots:** banks use placeholder classes. OpenPlay's typed roots are on `origin/OpenPlay` 57d4f1aa (untyped rows: `untyped_roots.tsv` at 4dd7257c). A2 is checked only as above.
7. **Coverage:** needs the tick to translate (its closure's unmodellable `1404FEDC0` / import slot), plus traced calls from the other 49, which the tick does not reach yet.

## 5. Verification

- Tests: none new (harness tooling). Gate not run (agents do not run it).
- Runs: `capture_frame.py` (4 accepts, rejections above); `frame_trace.py` (1, 2, 3, 6, 41 frames across configs); `diag_order.py`, `diag_args.py`, `probe_*.py` (scratch); `driver.py --only 1404F6C30` (rc 0, translated 0).
- Mutations: none (no engine code changed).
- Commit: nothing new since `02b03e81` (verified clean). Untracked `__pycache__` dirs are root-owned from sudo runs and were not staged.
