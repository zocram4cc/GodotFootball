

> **Main audit 19-10-26:** `0x14072EEFD` is the call inside FUN_14072EA00 (the rise blend set): matches. No fixed get-up timer: the next slide/tackle is gated by the current clip's +0x70 frame test (FallDown FUN_14072E360, Sliding FUN_140787EC0, Tackle FUN_140790160).
> **Main audit 19-10-26 (round 20):** `0x1406EADE0` `lea rax,[rcx+0x19a4]; ret` on the secondary (anime+0xAB8): the phase byte is anime+0x245C. The release at the stand-up end is not found statically (needs a live match); PARKED.
> **Main 19-10-26 (live):** one fall captured: Stagger 16 -> FallDown 17 (phase 1 constant) -> action 80 InplayDemoContact -> Move 2; the release is NOT a phase-2 release. CORRECTION: the current action id is the byte at anime_primary+0xAC9 (secondary+0x11), not anime+0x11.
## 12. Live check (round 26): one fall, and a correction to the action-id offset

**Correction (CONFIRMED live, supersedes §7 and round 16).** The current action id is the signed byte at **anime_primary + 0xAC9** (= secondary + 0x11). `FUN_1406E66B0` (`movsx eax, byte [rcx+0x11]`) is called on the secondary object (`anime+0xAB8`), so the byte is `anime+0xAB8+0x11`. My earlier reads at `anime+0x11` were the primary object and were noise. Verified: the corrected offset gives stable small values (2 = Move, 3 = StepMove) in a live match; the old offset gave random values. The clip id is the u16 at `anime+0xAE0` (`FUN_1406E6BD0` returns secondary+0x28). The phase byte `anime+0x245C` (= secondary+0x19A4) is unchanged and correct. The admission rule in §6 is unchanged; only its address is corrected.

**Live observation (CONFIRMED, `fallrec2.py`, one fall in ~300 s of play).** The first anime to enter FallDown (action 17):
- transitions: Stagger (16) at frame 45774 → FallDown (17) at ~46305 → **action 80 (InplayDemoContact per the RTTI list)** at ~46400 → action 2 (Move) at ~46485;
- during FallDown (46305–46395, 32 records): **phase byte = 1 (constant)**, clip id = 3788 (constant);
- the release is therefore **not** a phase-2 release; the byte never reached 2 in this fall.

**Open, and why the frame test cannot be checked.** The motion-table read returned `count = 0` for every record (base `anime+0x961` = secondary−0x157, the base `FUN_1406E68C0` receives; count at `+0x420`). So the remaining-frame value is 0 in the log and does not explain the release. Either the base or the table field is wrong. The release at ~46400 happened while the table read said 0 remaining and the phase was 1; under §4's rule (phase ≠ 2 selects the last entry, compared with `sec+0x38`) a release would need `whole-clip frames ≤ 0`, which the logged state does not give. So §4's frame test does not explain this release. The release must come from another path (the action change to 80 is decided elsewhere; not decoded).

**What is measured vs not:** measured: the action sequence 16 → 17 → 80 → 2 and the constant phase 1 / clip 3788 during the fall. Not measured: the entry frames and the release branch (the table base is unverified). Next: validate the table base (read `anime+0xAB8` sec vtable slot `+0x38` return value directly, which is `FUN_1406E6960`'s result, instead of the raw table) and record a fall with phase transitions.
