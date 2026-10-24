# PES diff-harness (emulator side): design, runs, specs, can't-run-yet list

`run.py` + `specs/*.json` → `corpora/<fn>.jsonl`. Scratch only; corpora are
PES-derived and never committed. Runner contract for DiffHarness:
`run.py --exe PATH --spec specs/<fn>.json --out OUT.jsonl [--only N]`
(python from `$GF_DIFF_PYTHON`, else python3; needs unicorn 2.1).
`PATH` is an **unpacked** image (file offset = VA − base); a packed
`PES2021.exe` is refused with a message. Base default `0x140000000`.

## 1. Design (`run.py`, ~460 lines)

- Whole image mapped copy-on-write from the file; `.rdata`/`.data` constants
  are the real bytes. Declared heap regions zero-filled per case, then `fills`
  + `absfills` (computed `region+off`) + `globals` (image VA → region) applied.
- Windows x64: `regs` (RCX/RDX/R8/R9 + XMM0-3 as full 128-bit ints), stack with
  `[RSP]=TRAP` ret + shadow + typed `stack` args at `+0x28`.
- Callee stubs by VA, attached at callee entry (CALL already pushed the
  return; skip = pop return into RIP): `noop`, `ret <val>`, `ret_f32 <val>`,
  `py <name>` (recorded draws, sheet getter, counter, bitfield writers).
- Unmapped read/write/fetch aborts the run with the faulting VA (an
  unmodelled global or missed stub) instead of silently continuing — a corpus
  line only lands on a clean run. Stop = return to TRAP page. Deterministic:
  re-runs are byte-identical (`cmp` clean on all three corpora).

Value language: ints, `"REGION"` / `"REGION+0xOFF"`, `"INPUT:name"`,
`{"f32in": name}` / `{"bits": ...}` (floats recorded as IEEE-754 bit patterns
in both inputs and outputs, so comparison is exact). `out`: `reg`, `xmm0f`,
`mem [region, off, type]`, `draws` (RNG log), `getters` (getter key log).

## 2. Proven functions

### cone `FUN_140A53DF0` — 410 lines, 0 failures, deterministic
- Spec: CAND/HOLD are 16-byte pos structs (`x`@+0, `z`@+8, ground plane);
  `RDX`=holder, `R8`=candidate, `XMM3`=head-deg, stack `coneDeg=90.0`,
  `outerLen=12.0`, `innerWord` (`0x00000000`, plus T7a's quirk
  `0x00000001`≈1.4e-45). No stubs — bearing/atan2/polynomial run real.
- Corpus: boundary edges (ahead/behind, outer touch `12.0` reject,
  just-inside hit, inner touch reject, half-angle edge), head flips
  (±180), zero-inner coin, wrap case, 60 aimed (offsets
  0/10/20/30/44/46/60/90/180° around the true bearing) + 340 random.
  39 hits / 371 rejects. Hand-checks: `(0,-10)` head 0 → 1; `(0,10)` → 0;
  outer touch → 0; just-inside → 1.

### band `FUN_140AF2DA0` — 326 lines, 0 failures, deterministic
- Spec: REC (0x17C player-edit record), EDIT (edit-mgr; `+0x48`→CNT which
  carries the `+0xD0BCF8` counter — the `FUN_1414BCCD0` increment is modelled
  for real), P (param_3 `0x12345678`), real image `.data` reads kept.
  Stubbed: the cond thunk `0x1414E2120` (returns input `b`), the `0x30`
  bitfield writer (replicates `rec[6]=(rec[6]&0x8FFFFFFF)|((band&7)<<28)`),
  `0x14150EC40→0`, C-runtime/frame teardown (`0x1412E2640→0` etc).
- Corpus: all 7 edges ±1 (`0x40/41, 0x46, 0x4B, 0x50, 0x55, 0x5A, 0x5F/60`),
  `0x00/0x28/0x6D/0xFF`, 300 random `b` × random `counter0`. Every line's
  `(w6>>28)&7` equals the decode ladder; `cond_echo==b`; `form_echo==0x14`
  (constant store, documented). Stub VA pitfalls fixed along the way:
  the thunk is `0x1415130F0→JMP 0x1414E2120` (hook the target), and the
  `0x1412D2640`/`0x1412E2640` typo.

### strategy `FUN_14051E970` — 403 lines, 0 failures, deterministic
- Spec: STRAT (side/mode/strat3-6/GameRandom `rng2`@+0x28), P2 (`+0x18`→MA,
  `+0xE0`→sheet chain), MA, MI (`116C/1168`), G (`C404` own/challenger
  floats, `C319` gate), GROOT (`+0x270`→MI, `+0x2AD0`→G), L11T sheet base,
  GOBJ team structs.
- Real: `FUN_14051E5B0` (score helper) and the CRT/mutex wrappers as `noop`;
  stubbed: sheet getter (keyed `blk,idx,col`, actual keys logged to
  `getters_seen`), `maskbit`, team-word fetch, support counts, clock/minute,
  GameRandom `0x140A924E0` computed in Python (LCG + float32 path, logged to
  `draws` with stream/range/result/state; hand-verified against the decode).
- Gotchas recorded in `spec.notes`: sheet col comes from the R8 register
  residue (arm1 loop `R8D=2`, arms 2/3/4 see entry `r8_entry`, default 0);
  the MA byte is at `MA+mode+side*8+0x25FF4` (no `0x18` — R9 already holds
  `[P2+0x18]`); RNG draws happen iff `minute==0 && mi1168<2 && strat5>0`;
  `side` is 0|1 only (side=2 reads `[0+4]`; excluded, PES never passes 2).
- Corpus: entry gate, arm-2/3/4/1 boundaries, full mode×side×period sweep,
  300 randoms. strat3 covers {0,1,2,3,4} (+9 untouched gate case),
  19 cases with draws.

## 3. How to run

```
$GF_DIFF_PYTHON run.py --exe ~/ghidra-projects/scratch/Unpacker/pes_full.bin \
  --spec specs/cone.json --out /tmp/c.jsonl        # or band / strategy
```

## 4. Can't-run-yet (audit order)

1. **Mark table (T7a `FUN_1404F61E0`)**: needs the live team tick's T object
   (`+0xB7D8/B7EC/markTable`, NL rows at `R8+0x22E4C`, pair writer S1 state),
   the `+0x2AA8/+0x18` ramp-base object, `pl+0x5D0` action word, `+0x5EC`
   head-vector semantics, and the `4EAF0/4E7C0` count gate decode — five
   unmodelled heap objects/globals, not just stubs.
2. **Mentality lane line**: needs RoleArrays/Tick context plus the mode-0
   rank inputs the lane slices haven't closed (block-B fold rule,
   promotion writer) — same heap-context problem, larger.
3. **Run slots**: needs the full slot-record array + designation publisher
   state (unmapped writers, live tick ordering).
4. **Team-tick steps**: needs the whole `FUN_1404F6C30` frame (MI/T/G/MA
   simultaneously, phase gates, S1-pass state) — the harness can map the
   regions but nobody has enumerated the tick's input closure yet.
