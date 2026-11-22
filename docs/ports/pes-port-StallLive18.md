# pes-port-StallLive18: per-code input comparison for refusal codes 7, 0, 3: blocked by missing real captures; nothing landed (StallLive, 21-11-26)

Status: BLOCKED on evidence, nothing landed. Branch `StallLive`, HEAD **1b56b7264df7b8984707f3936fb245ae45d5e2e1** (ls-remote confirmed; no commit). Tree equals HEAD. Build dir deleted.

## 1. What the chain returns for each code (CONFIRMED from `pes-re-trap-kind0.md`)
- **status 7**: the candidate query `FUN_14084DF20` finds zero candidates (step 5 of `FUN_14079AFD0`). Query inputs: the anime record, the ball sample, the action class (`FUN_14084C020`/`FUN_14084ECA0(…,0x15)`). Query builder reads are UNREAD in part.
- **status 0 (no status write, return 0)**: the race refusal `FUN_140799440` → `FUN_14079FBC0` (step 6). Its inputs: each teammate's predicted position (`MA+0x22E4C + id·200`), arrival `FUN_140A4D340`, and the ball sample `FUN_140A6CF70(ball, t)`. Meaning INF.
- **status 3**: the bearing predicate `FUN_14085C070` for the kind-3 branch, `|wrap180(bearing(anime velocity) − query+0x20)| ≤ 50` (step 8), or a commit failure (step 10).

## 2. What the captures give (MEASURED)
- Real captures available on disk: `trap_oracle` directories built from `pid3727195` seq025 and seq027 (both reach the chain; PES reference out = 1 and 4). The DribbleProposal oracle directories (`pid463316_seq4469`, `pid2580963_seq069`, `pid2670420_seq1396`) have dead `mem.bin` links (their captures are gone), so the oracle test skips them.
- So the only real reference outcomes available are out=1 (reach, status 1) and out=4 (height class, status 4). **There is no real capture with status 7, status 0 (race) or status 3** on disk. The rig's kind-0 captures named in `pes-re-trap-inputs.md` section 4 (seq4469, seq4531, seq23879, seq23906, seq23932) are deleted or unreadable.
- Therefore a forced GF case of codes 7, 0 or 3 has nothing real to compare against. The differing-field search requires a real capture of the same situation, which does not exist here.

## 3. Live sensitivity from round 17 (MEASURED, still valid)
Zeroing teammates' or the caller's anime class byte, or P+0x377C, changes 0 of ~5,071 live kind-0 outcomes. Zeroing the P+0x2FC velocity ring changes 162 (3.2%). Race and query codes are not explained by those fields.

## 4. What is needed (numbered)
1. A rig capture of `trapopen_14079A470` (existing hook) at a resting-ball refusal with code 7, 0 or 3. Predicate: `d(rdx+0x2458)`-free; simply `(issued flag == 0)` at a ball with speed < 0.3 m/s and a man within 3 m. Ask: arm a hook with cap 6 and keep FULL. Without it no field can be compared.
2. With such a capture: dump the query record (`FUN_14084DF20` writes +0x64..+0x9C, per `pes-re-record-layouts-chains.md` line 68) and the teammate arrival list (`MA+0x22E4C`) for the GF case and the capture, and diff the fields the query and race read.
3. The ring open item from round 17b (D/(10(k+1)) structure; contradiction with `pes-re-ball-track.md` section 3) carried forward.

## 5. Not done
- No red test, no fix. A fix without the real reference would be the unverified feed the brief forbids.
- Codes 2, 4, 5, 10, 11 not decoded (not needed until the dominant ones are explained).
