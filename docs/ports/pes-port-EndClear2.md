# PES Clear unit (kind 1): translation and proof status, GF wiring not done (EndClear2)

Branch `EndClear`, worktree `<workdrive>/gf-EndClear`, pushed commit `1c719193` (`git ls-remote origin EndClear` = `1c719193af58153ebf2cac7bbc4739ccf5e2e75e`), on base `origin/main` 970c8b91. Tags: CONFIRMED (capstone on `pes_full.bin`, harness run, or repo code at this commit), INF (inference), OPEN.

## 0. Verdict

- Step 1 is partly done. Three functions are now bit-exact and committed: `0x14063A370` (64/64), `0x14063C560` (57 bit-exact, 7 cases skipped by the harness), and `0x14063C9F0` (already recorded bit-exact on main; re-run 99/99). The Clear unit `0x142109EB0`, the corridor angle `0x142108630`, `0x14063A290`, `0x14063A490` and packer B `0x14063D2F0` are NOT proven. Their specs fail on every case.
- Steps 2, 3, 4 and 5 are NOT done. No GF code changed. There is no world-window extension, no danger writer, no `FillUnitTable` kind-1 wiring, no `forcedClear`, no stand-in deletion, no red test, no mutation, and no smoke. The Clear unit cannot run in the carrier until the angle is proven and committed.
- Nothing is faked. Each unproven function has its exact failure cause in §3.

## 1. What is committed (commit `1c719193`)

| VA | Result | Cases | Committed |
|---|---|---|---|
| 0x14063C9F0 | bit-exact (`results.json`, already recorded) | 99/99 re-run | unchanged |
| 0x14063A370 | bit-exact | 64/64 | `src/pes/translated/ai/pes_fn_14063A370.cpp`, `results_core.json` row |
| 0x14063C560 | bit-exact | 57 bit-exact, 7 skipped (harness: unmodelled read) | `src/pes/translated/ai/pes_fn_14063C560.cpp`, `results_core.json` row |

- Harness case source: all cases are synthetic (pilot-mode specgen specs, seeded by the spec). No real-call bank reaches these three (see §4).
- Specs added to `tools/dev/diffharness/specs/core/`: `142109EB0.json`, `142108630.json`, `14063C560.json`, `14063A290.json`, `14063A370.json`, `14063A490.json`.
- Layer: `translated.hpp` and `pes_dispatch_gen.hpp` regenerated. The other 3951 committed functions are byte-identical after regeneration.

Checks (run by me, because the assignment required them):
- `gen_translated.py --check`: `3953 functions (3674 recorded bit-exact, 279 closure) regenerate byte-identical`, rc 0.
- `driver.py --check --record results_core.json --spec-dir specs/core`: `all 2253 recorded-diffed functions still diff bit-exact`, rc 0. Took 9146 s (about 2.5 h).
- `cmake --build build --target pestranslated`: built, rc 0, after reconfigure (the new `ai/` files are picked up by `CONFIGURE_DEPENDS` glob only after reconfigure).
- Test binaries: none run. Nothing GF-side changed, and no test was written, so there is no red/green pair and no mutation to report.
- Gate: not run (Main gates).

## 2. Decoded facts (CONFIRMED, capstone on `pes_full.bin`)

### 2.1 Clear unit `0x142109EB0`, exact bytes

```
push rbx; sub rsp,0x20
mov rax,[r8+0x28]          ; imageA
mov r9,r8                  ; r9 = unit struct
mov rbx,rdx                ; rbx = answer out
cmp byte [rax+0x31C1],0    ; danger
je  0x142109EE5            ; no danger: write nothing
mov dword [rdx],5          ; answer type 5 (clear)
mov rax,[r8]               ; carrier record pointer
xor r8d,r8d                ; side = 0
mov rcx,[r9+8]             ; ctx
mov edx,[rax]              ; carrier id (u32 at carrier record +0)
call 0x142108630           ; corridor angle, result in xmm0
movss [rbx+8],xmm0         ; answer.a = angle
```

- Unit struct (r8, r9 identical on entry): `+0` carrier record pointer (its `+0` u32 is the carrier id), `+0x8` ctx pointer, `+0x28` imageA pointer. Answer (rdx): `+0` type u32, `+0x8` f32 angle.
- Correction to the port doc (`pes-port-EndClear.md` §1): its listing dropped `mov r9,r8` and `mov rbx,rdx`. The doc's "(r9 = r8)" is right.

### 2.2 Corridor angle `0x142108630` entry (prologue, CONFIRMED)

```
mov [rsp+0x70],r8b           ; side argument (u8)
mov [rsp+0x74],edx           ; carrier id argument (u32)
mov rbx,rcx                  ; ctx argument
mov rcx,[rip+0x15eb6e0]      ; = [0x1436F3DC8], the match global G (RIP-relative, resolves to 0x1436F3DC8)
mov rdi,[rcx+0x40]           ; G+0x40  (carrier record)
mov rax,[rcx+0x2aa8]         ; G+0x2AA8
```

- The angle takes ctx in rcx, carrier id in edx and side in r8b. The generated spec only declares `rcx`, so its carrier-id and side inputs are missing. This is the cause of the spec failure in §3.

### 2.3 Other functions in the chain (CONFIRMED by harness failure messages)

- `0x14063D2F0`: spec draws random u8 values for `G+0x270`, `G+0x0` and others. Those are pointer slots. The first failure is `unmodelled read at 0x35`.
- `0x14063A290`, `0x14063A490`: fail at addresses like `0x23a5f` and `0x23ad0`, read through pointer chains the static spec did not link.
- `0x142108630`: `G+0x180` is a `u64` input (a table pointer, stride `0x74` per the port doc) with no region. `A0+0xE0` is a `u64` input with no region. Neither is linked as a pointer.

## 3. Proof status of the chain (CONFIRMED by harness output)

| VA | Status | Result | Cause |
|---|---|---|---|
| 0x142109EB0 | not proven | 2 valid, 67 failed of 64 requested | carrier id domain; r9 fix applied in spec (r9 = r8 decoded); angle callee's own reads fail first |
| 0x142108630 | not proven | 0 valid, 128 failed | rdx (carrier id) and r8 (side) not in spec args; `G+0x180` and `A0+0xE0` pointers unmodelled; generator reports 917 unresolved memory operands |
| 0x14063A290 | not proven | 0 valid, 64 failed | pointer chains into `G` not linked |
| 0x14063A490 | not proven | 0 valid, 128 failed | same |
| 0x14063D2F0 | not proven | 0 valid, 68 failed (core spec, status "stand-in") | pointer slots drawn as u8 |
| 0x140699960 | not touched | - | - |
| 0x140A4C820 (forcedClear count test) | not decoded | - | - |

- Spec-generation facts: `0x142109EB0`, `0x142108630` and `0x14063A290` are not rows in `tools/dev/ledger/universe.json`, so `specgen generate` cannot find them. I generated their specs with a scratch wrapper outside the repo. It supplies sizes: Clear `0x36` (ret at `0x142109EE5`), angle `0x187A` (body to `0x142109EA9`, from the port doc), `0x14063A290` `0xD5` (to the next known function start). The wrapper has machine paths, so it is not committed.
- The angle spec's `precheck.calls` list equals the Clear spec's list (`14063A290, 14063A370, 14063A490, 14063C560, 142108630`). That is not the angle's own call set, so the precheck for `142108630` is unreliable. INF: a generator bug. Not investigated.

## 4. Case sources

- All current cases are synthetic (specgen pilot mode, seeded). The rule "prefer real-call cases" cannot be met yet. The live rendezvous hooks (`<workdrive>/PES21/rendezvous/hooks.cfg`, 25 hooks) include none at `0x142109EB0`, `0x142108630`, `0x14063A290` or `0x14063A490`. The captures in `<workdrive>/gf-Rendezvous-scratch/live2/` therefore cannot seed these functions. 
- Owner or Main action needed: add two entry hooks to `hooks.cfg`: `0x142109EB0` (Clear) and `0x142108630` (angle). Expected bytes for the hook line come from the image. I did not edit `hooks.cfg`.

## 5. Port-doc corrections

- `pes-port-EndClear.md` §4 and §8 item 2: `0x14063C9F0` is already a bit-exact record in `results.json` (99 cases, same row I re-ran). The doc said "no diffed row". Wrong.
- §1: the listing omits `mov r9,r8` and `mov rbx,rdx`. Added in §2.1 above.
- §3 and §5.3: the angle's carrier-id and side arguments are in edx and r8b (§2.2). The doc only mentions ctx.

## 6. Open items (numbered)

1. Hook request to Main: entry hooks at `0x142109EB0` and `0x142108630` in `hooks.cfg` so real-call banks can seed the Clear chain (§4).
2. Fix the angle spec: add `scalar_rdx` (u32, carrier id domain 0..21: the 22 on-pitch slots from `pesworldrecord.hpp`) and `scalar_r8` (u8, domain 0..1); add regions for `G+0x180` (table, stride `0x74`) and `A0+0xE0`; re-run `0x142108630`.
3. Fix the Clear spec's carrier-id domain (0..21); re-run `0x142109EB0`. Danger = 0 cases are only a trivial proof, so both danger arms must be exercised.
4. Fix pointer slots in `0x14063A290`, `0x14063A490`, `0x14063D2F0` (§3). Read each failing address back to its pointer source.
5. Decode `0x140699960` (danger writer, imageA+0x31C1) and `0x140A4C820` (forcedClear count test). Neither is started.
6. Step 2 (world window): the angle reads `G+0x40`, `G+0x180`, `G+0x270`, `G+0x360`, `G+0x2AA8`, `A0+0x18`, `A0+0xE0` and the imageA byte `+0x31C1`. `pesworldrecord.hpp` carries only G's header tables and the player blocks. Lay these out in PES order in the window and map `0x1436F3DC8` through `PesMem::segs`. Coordinate with FoulSwitch2 on `pesworldrecord*`.
7. Steps 3–5 (danger writer port, `FillUnitTable` kind 1, `forcedClear`, red/green TDD, mutation, 2-min seed-7 smoke in slot 2 or 3): blocked on 1–6.

## 7. Blockers for Main

- Hook request (item 1).
- Ledger universe lacks rows for `0x142109EB0`, `0x142108630`, `0x14063A290`. The specgen path needs them. Either Main adds the rows, or the spec generator takes sizes from the image.
