#!/usr/bin/env python3
"""PES hook plan (static): map each target VA to its function (.pdata), find direct callers, run the
window check (rv_hooks.displaced_window), and scan the whole code section for branches into each hook
window. Reads pes_full.bin only (flat image, file offset = VA - base). No live process is touched.

usage: hook_plan.py IMAGE OUT_DIR
writes OUT_DIR/plan.json (no instruction bytes) and OUT_DIR/pes_hooks.cfg (expected bytes: scratch only).
"""
import bisect
import json
import os
import re
import struct
import sys

import capstone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rv_hooks as rh  # noqa: E402

TEXT_RVA, TEXT_END_RVA = 0x1000, 0x1000 + 0x252D800   # .trace (code) section, from the PE table

# (name, VA, note). Targets given in the brief; the caller of the FixDemo evaluator is found below.
TARGETS = [
    ("cut_create_141F57840", 0x141F57840, "cutscene CREATE/LOAD (brief)"),
    ("cut_load_141F36200", 0x141F36200, "cutscene CREATE/LOAD (brief)"),
    ("cut_load_141F5F330", 0x141F5F330, "cutscene CREATE/LOAD (brief); ledger: slot-list build"),
    ("fixdemo_eval_141F4D2F0", 0x141F4D2F0, "FixDemo evaluator (brief; ledger: condition evaluator)"),
    ("fixdemo_match_141F4B1D0", 0x141F4B1D0, "FixDemo matcher (scene-birth doc, not in brief)"),
    ("restart_44C880", 0x14044C880, "restart builder (brief; situation-sequencer runner)"),
    ("restart_446390", 0x140446390, "restart builder (brief; fan-in)"),
    ("camera_update_87F280", 0x14087F280, "in-play camera update (brief; vtable slot 4)"),
    ("camera_caller_886780", 0x140886780, "caller of the camera update (brief)"),
    ("hud_radar_update_E2910", 0x1420E2910, "HUD radar update (hud-ui doc §5/§9)"),
    ("mi_refresh_113B280", 0x14113B280, "per-tick MI refresh candidate; brief VA 0x14113B27F is int3 padding before it"),
    ("mi_refresh_113B520", 0x14113B520, "per-tick MI refresh candidate; brief VA 0x14113B51F is int3 padding before it"),
    ("type13_body_A115E9", 0x140A115E9, "type-13 body: its function (brief, for G+0x180)"),
    # Foul/offside stoppage functions that need real-call seeds (docs/ports/pes-port-FoulOffside.md).
    ("foul_sanction_47F170", 0x14047F170, "sanction -> scene decision tree"),
    ("foul_sanction_caller_480910", 0x140480910, "sanction tree's caller"),
    ("ref_involve_A3A690", 0x140A3A690, "officials' involvement"),
    ("ref_judge_A3A930", 0x140A3A930, "referee JUDGE action"),
    ("ref_latch_A3A870", 0x140A3A870, "one-signal-per-stop latch"),
    ("ref_restart_cmd_A39A80", 0x140A39A80, "restart command"),
    ("linesman_lookat_A38F20", 0x140A38F20, "linesman look-at"),
    ("restart_driver_445E90", 0x140445E90, "restart step-programme driver"),
    # Kick scatter and press command: real-call cases (docs/ports/pes-port-KickInputs.md, docs/re/pes-re-press-live.md).
    # Not hookable, so reached through the scatter entry: the quality 0x1408413C0 (rip-relative operand in the window)
    # and the scatter body at 0x1408398D5 (a call in the window).
    ("kick_scatter_83CC30", 0x14083CC30, "kick scatter entry"),
    ("kick_postscale_8414A0", 0x1408414A0, "kick quality post-scales"),
    ("press_cmd_writer_A85A70", 0x140A85A70, "press command words writer (class, useClassRate, kmh)"),
    # The carrier's Clear unit and its corridor angle (docs/ports/pes-port-EndClear2.md).
    ("carrier_clear_2109EB0", 0x142109EB0, "carrier Clear unit (kind 1)"),
    ("clear_angle_2108630", 0x142108630, "clear corridor angle"),
    # The Trap action's kind-0 control-touch chain (docs/re/pes-re-trap-kind0.md, docs/ports/pes-port-RecordMap.md).
    ("trap_kind0_79A470", 0x14079A470, "Trap action kind-0 control touch"),
    ("trap_commit_79AFD0", 0x14079AFD0, "Trap clip commit step"),
    # Clear chain inputs and the cutscene camera's cut offset (docs/ports/pes-port-MatchInfo.md, -FdcCamera4.md).
    ("clear_helper_63A490", 0x14063A490, "Clear chain helper"),
    ("danger_writer_699960", 0x140699960, "carrier danger flag writer"),
    ("danger_builder_2119390", 0x142119390, "imageA builder that stores the danger byte"),
    ("cut_offset_1F39CC0", 0x141F39CC0, "cutscene player: cut offset and the played camera key"),
    # FixDemo selection inputs for the GF-side request (docs/ports/pes-port-CutsceneHybrid2.md, CutsceneHybrid3).
    ("fixdemo_wrapper_141F4FDA0", 0x141F4FDA0, "FixDemo variant wrapper"),
    # The ctx setter 0x141F51630 is not hookable (a je at +3 falls inside the patch window).
    ("fixdemo_ctx_init_141F514D0", 0x141F514D0, "FixDemo ctx init"),
    ("goal_pick_140477400", 0x140477400, "goal cutscene pool pick"),
    # The carrier's unit walk: the forced-clear decision is inline here (docs/ports/pes-port-MatchInfo3.md).
    ("carrier_walk_210F110", 0x14210F110, "carrier unit walk (forced clear)"),
    # The danger writer's input pressure record (MatchInfo4): built right before 0x140699960 runs.
    ("pressure_build_69A500", 0x14069A500, "pressure record builder feeding the danger writer"),
    # FixDemo scene start (CutsceneCam): reads scene+0x110..0x120 and the step kind; goal/foul cut offsets.
    ("fixdemo_scene_start_141F5CD00", 0x141F5CD00, "FixDemo scene start (cut offset per scene)"),
    # Goal cutscene anchor (CutsceneCam): scorer position clamped to the pitch, then written to scene+0x110..0x120.
    ("cut_anchor_calc_474F00", 0x140474F00, "goal scene anchor from the scorer's position"),
    ("cut_anchor_write_47D280", 0x14047D280, "goal scene anchor writer (scene+0x110..0x120)"),
    # The foul judge's level decision (docs/re/pes-re-duels-fouls.md 2.2): contact -> points -> level -> pending foul (CardLevel).
    ("judge_gate_50D6A0", 0x14050D6A0, "judge contact gate (in play, both ids < 22, B played the ball / targeted A)"),
    ("judge_decide_5B7470", 0x1405B7470, "judge level decision: points, thresholds, overrides, builds the pending foul"),
    ("judge_pending_5B8000", 0x1405B8000, "builds the pending foul record (restart, kind, level, points)"),
    ("judge_setpending_50EE40", 0x14050EE40, "Judge::SetPending"),
    ("judge_whistle_50E770", 0x14050E770, "whistle writer: JR restart/side/reason/spot/kind/fouler/victim"),
    ("judge_commit_43F546F0", 0x143F546F0, "Judge::Commit (protected): JR+0x38 card level"),
]


def load_pdata(img):
    exc_rva, exc_size = img.exc
    funcs = []
    for i in range(exc_size // 12):
        b, e, _ = struct.unpack_from("<III", img.data, exc_rva + 12 * i)
        if b:
            funcs.append((b, e))
    funcs.sort()
    return funcs


def function_of(funcs, starts, rva):
    i = bisect.bisect_right(starts, rva) - 1
    if i >= 0 and funcs[i][0] <= rva < funcs[i][1]:
        return funcs[i]
    return None


def callers_of(img, blob, funcs, starts, fn_rva):
    """Direct E8 calls to fn_rva whose instruction is confirmed by a linear decode of the caller."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    out = {}
    for m in re.finditer(rb"\xe8", blob):
        i = m.start()
        if i + 5 > len(blob):
            continue
        rel = struct.unpack_from("<i", blob, i + 1)[0]
        rva = TEXT_RVA + i
        if rva + 5 + rel != fn_rva:
            continue
        fn = function_of(funcs, starts, rva)
        if fn is None:
            out.setdefault("unmapped", []).append(hex(img.base + rva))
            continue
        b, e = fn
        code = blob[b - TEXT_RVA:e - TEXT_RVA]
        confirmed = any(ins.address == img.base + rva and ins.mnemonic == "call"
                        for ins in md.disasm(code, img.base + b))
        if confirmed:
            out.setdefault(hex(img.base + b), []).append(hex(img.base + rva))
    return out


def branch_candidates(blob):
    """Raw-byte branch candidates in the code section: {target RVA: [source RVA, ...]} for rel32 call/jmp/jcc
    and rel8 jmp/jcc. Conservative: a data byte can make a candidate; an instruction cannot be missed."""
    cand = {}

    def add(src, tgt):
        cand.setdefault(tgt, []).append(src)

    for m in re.finditer(rb"[\xe8\xe9]", blob):
        i = m.start()
        if i + 5 <= len(blob):
            add(TEXT_RVA + i, TEXT_RVA + i + 5 + struct.unpack_from("<i", blob, i + 1)[0])
    for m in re.finditer(rb"\x0f[\x80-\x8f]", blob):
        i = m.start()
        if i + 6 <= len(blob):
            add(TEXT_RVA + i, TEXT_RVA + i + 6 + struct.unpack_from("<i", blob, i + 2)[0])
    for m in re.finditer(rb"[\xeb\x70-\x7f]", blob):
        i = m.start()
        if i + 2 <= len(blob):
            add(TEXT_RVA + i, TEXT_RVA + i + 2 + struct.unpack_from("<b", blob, i + 1)[0])
    return cand


def real_branch_into(img, blob, funcs, starts, src_rva, tgt_rva):
    """True when a linear decode of the function holding src_rva has a direct jump/call at src_rva to tgt_rva."""
    fn = function_of(funcs, starts, src_rva)
    if fn is None:
        return False
    b, e = fn
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    for ins in md.disasm(blob[b - TEXT_RVA:e - TEXT_RVA], img.base + b):
        if ins.address == img.base + src_rva:
            if not (ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL)):
                return False
            imms = [o.imm for o in ins.operands if o.type == capstone.x86.X86_OP_IMM]
            return bool(imms) and imms[0] == img.base + tgt_rva
    return False


def main(argv):
    img_path, out_dir = argv[1], argv[2]
    os.makedirs(out_dir, exist_ok=True)
    img = rh.Image(img_path, flat=True)
    funcs = load_pdata(img)
    starts = [b for b, _ in funcs]
    blob = bytes(img.data[TEXT_RVA:TEXT_END_RVA])
    print("pdata functions: %d; code bytes scanned: %d" % (len(funcs), len(blob)))

    hooks, rows, cfg = [], [], ["# PES hook plan (scratch; expected bytes are PES code: never commit)"]
    seen = set()

    def add(name, va, note, entry_rva):
        if entry_rva in seen:
            return
        seen.add(entry_rva)
        entry = img.base + entry_rva
        code = img.read(entry, 64)
        length, why = rh.displaced_window(code, entry)
        row = {"name": name, "target": hex(va), "entry": hex(entry), "note": note,
               "is_entry": entry == va, "window": length, "refused": why}
        if length:
            row["insns"] = [i.mnemonic for i in capstone.Cs(capstone.CS_ARCH_X86,
                                                              capstone.CS_MODE_64).disasm(code[:length], entry)]
            hooks.append((name, entry, length, code[:length]))
        rows.append(row)

    for name, va, note in TARGETS:
        rva = va - img.base
        fn = function_of(funcs, starts, rva)
        if fn is None:
            rows.append({"name": name, "target": hex(va), "refused": "not inside any .pdata function"})
            continue
        add(name, va, note, fn[0])

    # Callers of the FixDemo evaluator (direct E8 calls, confirmed by decoding the caller).
    callers = callers_of(img, blob, funcs, starts, 0x141F4D2F0 - img.base)
    rows.append({"name": "fixdemo_eval_callers", "callers": callers})
    for caller_fn in sorted(k for k in callers if k.startswith("0x")):
        add("fixdemo_caller_" + caller_fn[2:], int(caller_fn, 16), "direct caller of the FixDemo evaluator",
            int(caller_fn, 16) - img.base)
    rows.append({"name": "camera_update_callers", "callers": callers_of(
        img, blob, funcs, starts, 0x14087F280 - img.base)})

    # Branches into each passing hook window: raw-byte candidates, each confirmed (or dismissed) by a linear
    # decode of the function that holds its source byte. A real branch into a window makes the hook unsafe.
    cand = branch_candidates(blob)
    for name, entry, length, _ in hooks:
        lo, hi = entry - img.base + 1, entry - img.base + length
        raw, real = [], []
        for tgt in sorted(t for t in cand if lo <= t < hi):
            for src in cand[tgt]:
                raw.append("%s->%s" % (hex(img.base + src), hex(img.base + tgt)))
                if real_branch_into(img, blob, funcs, starts, src, tgt):
                    real.append("%s->%s" % (hex(img.base + src), hex(img.base + tgt)))
        for row in rows:
            if row.get("entry") == hex(entry):
                row["raw_candidates_n"] = len(raw)
                row["raw_candidates"] = raw[:8]
                row["real_branches_into_window"] = real
    with open(os.path.join(out_dir, "plan.json"), "w") as f:
        json.dump(rows, f, indent=1, default=str)
    for name, entry, length, code in hooks:
        cfg.append("hook %s %#x %d 0 0 %s" % (name, entry, length, code.hex()))
    with open(os.path.join(out_dir, "pes_hooks.cfg"), "w") as f:
        f.write("\n".join(cfg) + "\n")
    for r in rows:
        if "entry" in r or "refused" in r:
            status = "PASS" if r.get("window") else "REFUSE"
            print("%-6s %-26s target=%s entry=%s %s %s" % (
                status, r["name"], r.get("target", "-"), r.get("entry", "-"),
                ("window=%d" % r["window"]) if r.get("window") else "",
                r.get("refused") or ""))
        elif "callers" in r:
            print("CALLERS %s: %s" % (r["name"], json.dumps(r["callers"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
