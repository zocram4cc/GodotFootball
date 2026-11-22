# pes-port-PresReplay19: the recorded-answer switch is test-only; a second-read hook recipe; the closure restored; the release test is red (round 18)

Branch `Presentation`, pushed `b697bca9..e1cae390` (`git ls-remote origin Presentation` = `e1cae3902437149a21e694936171af21037f7c75`). Build dir removed; unpacked captures removed (scratch 27 MB).

Process: the commit tool still fails in this worktree (`extern/godot-cpp` symlink); committed with explicit paths under the brief's identity (accepted by Main).

Labels: CONFIRMED = a PES replay (`frame_trace.run_recorded`, PES's own bytes), a test run, or a scan of the image; INF = inferred; OPEN = not settled.

## 1. The recorded-answer switch lives only in test mode (CONFIRMED, no leak)

- The switch is `PesHeap::ReplayAnswers` (`src/pes/translated/pes_native.cpp`). It is called from one place: `ReplayHumanFrame::Create(exe, err, pre, answers, record)` (`src/pes/replay_human.cpp`, the `m.heap->ReplayAnswers` line), and `Create` is called only by `tests/pes_replay_human_test.cpp` (`MakeFrameFrom`, reached only through the `GF_PES_COPY_CAPTURE` / `GF_PES_RELEASE_CAPTURE` gates).
- The runtime heaps never call it: `src/pes/fixdemo_select.cpp:164` and `src/pes/fixdemo_camera_rig.cpp:93` construct their own `PesHeap` and carve every block (no `ReplayAnswers`). `ReplayHumanFrame` has no runtime caller.
- The header comment now says TEST MODE ONLY and names the callers. The `Alloc` contract comment sits with `Alloc` again.
- Added in test mode only: a free of a block the heap did not hand out releases nothing (`foreignFreesOk_`, set by `ReplayAnswers`). The runtime heap still aborts on such a free (`PES_NATIVE_HEAP: free of a pointer this heap did not allocate`). Test: `PesNativePool.InTestModeAFreeOfABlockBeforeTheCallReleasesNothing`.

## 2. Second copy: the hook (CONFIRMED evidence; the predicate is INF until it runs on the rig)

Evidence from the captures (copy_livein.py replays, PES's bytes):

| capture | human (R+0x28) | palette byte | guard | R+0x48 at entry | copies? | copied entry vs seq205 |
|---|---|---|---|---|---|---|
| pid2024691 seq205 | 0 | 0 | 0xcddd | 0 | yes (3 xchgs, sources 22e504d0/22e50570) | — |
| pid2024691 seq206 | 1 | 1 | 0xf559 | 0 | yes (same sources) | 28 bytes differ |
| pid2024691 seq207 | 2 | 2 | 0xeb7d | 0 | yes | 23 bytes differ |
| pid2024691 seq208 | 3 | 3 | 0xec6b | 0 | yes | 27 bytes differ |
| pid2998104 seq567/960/1464/1954/1955 | 15, … | … | … | nonzero | **no** (stored entry equals palette) | — |
| pid2695300 seq015 | 15 | 15 | 0xfd68 | nonzero | no | — |

- A first read (`R+0x48 == 0`) creates the human's object and copies. Each human has its own record R, so seq206-208 are real copies with **different palette entries** from seq205 (different bytes after the call), but on different records.
- A second read (`R+0x48 != 0`) copies only when the stored entry differs from the palette entry. The five existing second reads did not copy (no real call in the set has a changed palette on a second read).
- The synthetic flip (stored byte 0 changed) is PES-replayed to a copy from other registry entries (sources `23ba0070`, `23ba0110`): `flip_seq567` copies and runs the release (PES's own after bytes written by copy_livein.py).

Recipe for Main to arm (the rv `--when` evaluator reads `rcx`, `rdx`, `r8` and `b/d/q(addr)`; verified offline on the captures above: `rcx` = the record R, `rdx` = the frame list L, `q(rdx+0x10)` = the frames, the human's record = frames + h*300, guard at +0x30C, palette byte at +0x36B):

1. **Second reads, every call** (the one that can change the entry): `hook replay_frame_14217DEF0 0x14217def0 15 1 <cap> 488974242041564883ec2080791800` with `--when 'replay_frame_14217DEF0=q(rcx+0x48)!=0'`, EVERY 1 (the armed line used EVERY 20, which samples the second reads too sparsely to catch a palette change). Offline filter: a call copies iff copy_livein.py finds `S` lines; its changed-entry signal is after.bin differing from the stored R+0x50 bytes at entry.
2. **First copy of a human with a palette index not seen so far** (a real copy with a palette entry that differs from seq205's, on a new record): `--when 'replay_frame_14217DEF0=q(rcx+0x48)==0 and b(q(rdx+0x10)+d(rcx+0x28)*300+0x36B)>3'`, cap 6 (humans 4+; humans 1-3 are already in hand).

Caveat (OPEN): (1) is only a rate estimate; the copy on a second read needs the human's palette index to have changed between reads, which the rig does not force. Nothing here has been armed or captured by me.

## 3. The sequence test restored; the release test (CONFIRMED: red when its capture is named)

- `ASequenceKeepsTheCopiedEntryAcrossTheReadsThatDoNotCopy` (gated on `GF_PES_COPY_CAPTURE`): the first read on seq205's own inputs (`FromCapture`: human, guard, active, palette from the I line) equals `after.bin`; the absent and inactive reads keep it; `word1c` -1 and `byte21` 0 after the copy. **PASS** with seq205.
- `AChangedEntryOnASecondReadReplacesTheCopiedEntryThroughTheRelease` (gated on `GF_PES_RELEASE_CAPTURE`, which names `synth_seq567/flip_seq567`): expects the PES after bytes and the release state. Default run: **SKIP** with the named reason. With the capture named: **ABORTS**: `PES_NATIVE_HEAP: free of a pointer this heap did not allocate` was the first failure (the release frees PES's pre-call object 0x372a80a0); the test-mode free rule fixed that; the next failure is a SIGSEGV in the region-manager recursion `pes_fn_1403AB470 <-> pes_fn_1403AB570` (`PesMem::wr`). CAUSE: INF: the port's TLS/teb windows are synthetic (`kTeb`, `kTls`, `kSlot`), while PES's region manager reads them through `gs:[0x58]`; the proof run (snapdiff over the whole PES memory) passes the same closure, so the difference is the test's pre-state, not the closure. OPEN: map PES's TLS/teb pages (or the slot block) into the test pre-state. This is a red test, committed red under its gate; it is not weakened.
- The test is not a second-copy-with-changed-entry claim on a real call: its input is the synthetic flip (PES-replayed).

## 4. Synthetic states re-run; the closure restored (CONFIRMED)

- Five synthetic copy states (`synth.py`, one per pid2998104 second read, `R+0x50` byte 0 flipped; spec `fields`/`cases`, case `stored0`) were re-run with the six real calls in one `prove_bounded` run, merged into `results_presentation.json`: **proven, 11 of 11 bit-exact, 0 mismatches, own-block 13/13**. The record notes the synthetic count.
- The six-real-call closure (277 members, the round-17 closure) is **not sufficient**: the synthetic states reach 86 functions it does not contain (among them `1408BE090`, the destructor the second copy runs). So the 82 members pruned in round 17 were **not dead**; the round-17 claim was wrong. The closure is restored to 362 members (the proof's member list; `closure_destr14217.json`).
- Real calls could not replace the synthetic ones: no captured second copy with a changed entry exists (section 2). Recorded as the reason.

## 5. Import models: per-call-site check (CONFIRMED); nothing deleted

Scan of all 5815 translated functions (capstone over `.pdata` ranges, `call`/`jmp qword ptr [rip+disp]` into the slot):

| slot (IAT) | export | call sites (translated function: instruction) | owning closure |
|---|---|---|---|
| 0x14252F4B0 | GetSystemInfo | 14038EE80 (14038EFE9), 1403FCC30 (1403FCCB6) | closure_trapdribble.json |
| 0x14252F6B0 | TlsAlloc | 14038EE80 (14038EF90) | closure_trapdribble.json |
| 0x14252F6D0 | VirtualFree | 14037FE70 (jmp 14037FE78), 1403B4E00, 1403B4F40, 1403B4FC0 (x2), 1403FC080 | closure_trapdribble.json |
| 0x14252F6D8 | VirtualAlloc | 1403B4E00, 1403B4EB0, 1403B4FA0, 1403B4FC0, 1403FC050 | closure_trapdribble.json |

Every call site is a member of `closure_trapdribble.json` (StallLive's closure, present in the layer). So the models are reached by the layer, and none is deleted. The Presentation closure (362 members) reaches none of them directly. OPEN: whether `closure_trapdribble.json` is live in its own proofs is StallLive's to answer; I did not touch it.

## 6. Checks

- Copy tests: `PES21_ROOT` + `GF_PES_COPY_CAPTURE=<seq205>`: **7/7 PASS** (the release test is excluded by the gate here).
- Without any capture: **5 PASS, 3 SKIP** (copy, sequence, release), each with a named reason.
- `pes_hud_clock_test` 4/4 PASS. `pes_native_service_test` 7/7 PASS (new: the recorded-answer test; the test-mode foreign free).
- Python: `test_create_path_models`, `test_import_keys`, `test_prove_bounded`, `test_native_services` OK. `ledger.py --check`: current (44459 rows).
- `gen_translated.py --check`: 5869 functions (3769 recorded bit-exact, 2100 closure), byte-identical.
- Mutations this round: none run (the switch change is a scope fix; the release test is red, not a mutation check).

## 7. Commit

- `e1cae390` on `Presentation` (pushed; ls-remote confirmed). Paths: `src/pes/translated/` (regenerated; includes `pes_native.{hpp,cpp}`), `tests/pes_replay_human_test.cpp`, `tests/pes_native_service_test.cpp`, `tools/dev/translate/{copy_livein.py,closure_destr14217.json,results_presentation.json}`.

## 8. Open items for Main

1. Arm the hook recipe in section 2 (predicate 1 at EVERY 1; predicate 2 for humans 4+).
2. The release test needs PES's TLS/teb state in the pre-state (section 3). Until then it is red under its gate.
3. `closure_trapdribble.json` owns the import call sites (section 5).
