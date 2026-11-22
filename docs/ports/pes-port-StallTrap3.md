# pes-port-StallTrap3: counter B's PES writer is decoded to its call chain; the chain is 6 untranslated functions deep; nothing landed, tree clean (StallTrap, 21-11-26)

Status: PARTIAL, NOTHING LANDED, NOTHING PUSHED. Branch `StallTrap` local at `646cc259` (StallRun a2439354 + FastStall 88ae7714). Worktree clean; `build/` deleted. Baseline PesStallQuick 58 forced ticks (unchanged). Not run: ALiveBall, PesHang, PesPassTarget, gen --check (no change to land).

Reason for stopping: the translation of the counter-B closure (section 3) is a multi-function pipeline job (closure, gen, snapdiff proof on captures), and the round's request budget ran out before the GF feed and red test could be written against a real PES value. A stand-in in the feed would be the same kind of guess the 21-11 rules forbid. This is the state to resume from.

## 1. Counter B's store (CONFIRMED, image bytes)

Image: `pes_full.bin` (flat; file offset = VA - 0x140000000). Ghidra's HTTP bridge (127.0.0.1:8080) is not running, so this was read with `objdump -D -b binary` on the flat image.

- `FUN_14092ED30` (slot-record builder, `r15` = slot record) stores counter B at **0x14092F204**: `41 89 87 38 14 00 00` = `mov [r15+0x1438], eax`.
- The value comes from `call 0x928120` at 0x92F1FF (args: `rcx`=out buffer, `rdx`=`rdi+0x18` (player id), `r8`=`rsi` (ball record), `r9`=`rdi+0xc8`, stack args: `[rsp+0x28]`=`rbp+0xf`, `[rsp+0x20]`=`rsi`). Counter A (`+0x1430`) is `[rsp+0x3c]` written at 0x92F1CF, not from 0x928120.
- `FUN_140928120` (0x928120–0x9281F2, 0x60-byte frame): takes the man's `+0x55C` (x), `+0x564` (z) and the ball point at `[rdi]`, `[rdi+8]`. If the ball's xz equals the man's xz, it sets xmm0 = 0 and skips to the call. Otherwise it computes `dz = ball.z − man.z`, `dx = ball.x − man.x`, calls `0xA92D40`, adds 360 (0x25795E4) if negative, multiplies by 0x259BE40. Then it calls `0x678950` with a bearing float and a 0/1/2 mode word (`[rsp+0x50] = 2`).
- **Return value:** the function's epilogue does not set `eax`. The `eax` the writer stores at 0x92F204 is whatever `call 0x678950` leaves in `eax`. This must be confirmed on the callee's return path, not assumed.

## 2. What the value is (INF until the chain is decoded)

The chain is a per-frame search over the man's motion (`0x678990`, the 11-sample loop at `0x678AC3`–`0x678B24` reads the man's `+0x55C/+0x564` at `0xA48A10(G, slot)` against the ball's samples `[r13+0x0]`, `[r13+0x8]` with a squared-distance test `0x25C6780`). This is the arrival/need estimate that PES feeds into counter B (pes-re-player-record.md 2.1: counter B 21..500 frames captured). GF's `timeNeededToGetToBall_ms` is a different function (AI_GetTimeNeededForDistance_ms) and is what team.cpp writes into SR+0x1438 today (×54/1000); the team.cpp ponytail says so.

## 3. Callee closure and layer coverage (measured)

Call targets of the chain, and whether `src/pes/translated` holds them (grep on `pes_fn_<VA>`):

| function | in layer | role (from the decode above) |
|---|---|---|
| 0x14092ED30 | no | slot-record builder; the counter-B store |
| 0x140928120 | no | counter-B value: bearing + call 0x678950 |
| 0x140678950 | no | thin wrapper, calls 0x6788B0 (args shuffled) |
| 0x1406788B0 | no | the per-frame search (not decoded here) |
| 0x140678990 | no | 11-sample loop over the man's position and the ball samples |
| 0x140677700, 0x140676A50 | no / 3 hits | (the 0x140677700 row in the ledger is undecoded) |
| 0x14124DAD0, 0x1401E532E0, 0x1401E532D0 | no | frame/time helpers (0x1E532E0 returns a value used as ×5 / ×3 scales) |
| 0x140A92D40 | yes (diffed, pesteamconcepts et al.) | bearing / atan2 (used by 0x928120) |
| 0x14163... 0x1863350 | no | a predicate returning 0/1 (used in 0x678990) |
| 0x140A927B0 | no | float helper |
| 0x140A4D350 | no | unit/arrival helper |
| 0x140439E50, 0x14043A0A0, 0x1404583B0, 0x14046F230 | yes | bearing / Vector helpers already in layer |
| 0x140A48A10, 0x140A48A30, 0x140A489F0, 0x140A3E190 | yes | record accessors (G, slot) |
| 0x140A57800, 0x140A57720, 0x140A57450, 0x140A57650, 0x140A57710 | yes | ball-sample readers |
| 0x140A6CF70, 0x140A62610, 0x140A62690, 0x140A627E0, 0x140A76240, 0x140A8FF10 | yes | ball prediction / contact helpers |

So the chain needs **six** translations that are not in the layer: 0x14092ED30, 0x140928120, 0x140678950, 0x1406788B0, 0x140678990, plus 0x14124DAD0, 0x1401E532E0, 0x1401E532D0, 0x1863350, 0x140677700 and 0x140A4D350 (about 11 undecoded or untranslated callees). Each goes through closure, gen and a snapdiff proof on the captures.

## 4. Captures for the proof

Not inspected in this round. The team-AI tick captures (`live2/ai_team_tick`, `teamsel`, `press`) are the candidates the owner named. The writer runs once per on-pitch slot per slot-record build, so a capture of the team tick where man slots are rebuilt should contain the 0x14092ED30 call. Unverified.

## 5. g1 (the race's constant record pointer)

Not decoded. Known facts (census5, CONFIRMED): `g1` = 0x2000188228 for every arrival call in one race (constant), it does not fall in any man/slot block (`BlockVa(s)` for s in 0..21 covers 0x2000102E58..0x200015 98B8), and it is the gpr12/gpr1 value the race's loop uses as a base (`gpr1 + gpr14*1`, race loop at 14079FBC0 line ~210). It is probably a side or team AI row base (MA/AI region), not a man. To settle: print `PesWorld::MaVa(side)` and `AiVa(side)` / `TeamInfoVa(side)` next to the arrival probe and match `g1` to one of them.

## 6. Next steps (exact, in order)

1. Decode `FUN_1406788B0` (the search) and `FUN_140678990`: which fields of the man record and the ball samples they read, and the return value path of 0x678950 (what `eax` is). Use `objdump` on the flat image (`--start-address=0x6788B0`).
2. Translate the closure through the normal pipeline (closure JSON root 0x14092ED30 or 0x140928120; gen_translated; `snapdiff` proof on a team-tick capture). Confirm `eax` at 0x92F204 bit-exact.
3. GF feed: `team.cpp` RunPesPassTarget writes SR+0x1438 from the translated writer's output, not `GetTimeNeededToGetToBall_ms()`. The shared `AI_GetTimeNeededForDistance_ms` stays unchanged.
4. Red-first test: a resting ball ≤1.5 m from the nearest man, the nearest man has the smallest SR+0x1438 and wins the race (the 14079FBC0 teammate test returns 0, no refusal).
5. Then PesStallQuick, full ALiveBall, PesHang, PesPassTarget, gen --check, push.

## 7. Killed or corrected

- The earlier claim "counter B is GF's time-to-ball" is CONFIRMED for the feed (team.cpp) but its PES counterpart is not `FUN_14092ED30`'s own arithmetic: that writer only stores what `FUN_140928120` returns.
- The ledger's "decoded" status for 0x14092ED30 is about its stores, not the value's arithmetic (0x928120's return is not visible in the ledger row).
