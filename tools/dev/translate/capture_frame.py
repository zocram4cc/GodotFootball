#!/usr/bin/env python3
"""Full-frame capture for the frame trace (read-only; FrameTrace).

capture_midplay.py reads only the spec-walked chains, so the team tick's callees reach
memory it never captured. This tool keeps its layout and its gates, and adds what the match
step touches: G (whole), MI, MA, H (the team-tick owner, *(0x1436F3F98), vtable 0x142597580),
M, both team records, the 22 on-pitch rows of the anime, man and slot tables, then up to
--depth rounds of heap-pointer following (image pointers are the dump's bytes).

Gates. Team +0x0 is a per-frame counter, so teams are checked by identity: the G slots that
name MI and both teams are unchanged across the reads. MI's header and H's vtable (first 8
bytes) are unchanged, and the MI frame counter advances by at most --frame-max. The G blob is
not torn (MI and team slots present). Output is capture_midplay's layout (g.bin,
<va>-<hi>.bin, live.json), so seed.py and the frame runner read it unchanged.

Usage: sudo -n python3 capture_frame.py --out DIR [--max-caps N] [--dur S] [--open-every S]
       [--frame-max N] [--depth N]
"""
import argparse
import bisect
import json
import os
import struct
import subprocess
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import capture_midplay as cm  # noqa: E402
import seed  # noqa: E402

H_GLOBAL, M_GLOBAL = cm.H_GLOBAL, cm.M_GLOBAL
H_VTABLE = 0x142597580      # team-tick owner H (pes-re-live-matchball.md, pes-re-gaps-a2.md)
MI_WIN, H_WIN, M_WIN = 0x8000, 0x10000, 0x2000
TEAM_WIN, ROW_WIN = 0x10000, 0x4000
DEEP_WIN = 0x1000           # window for an object reached by pointer following
MAX_OBJS = 1500             # pointer-followed objects per capture (the capture must fit ~2 frames)
MAX_BYTES = 32 << 20
STACK_LO = 0x7F0000000000   # user-space stacks and up: never heap objects


def heap_ptrs(buf):
    """Distinct 8-aligned qwords in buf that look like heap or data pointers (not image, not stack)."""
    n = len(buf) // 8
    if n == 0:
        return set()
    a = np.frombuffer(buf, dtype="<u8", count=n)
    m = a[(a >= 0x10000) & (a < STACK_LO)]
    m = m[(m < cm.IMAGE_BASE) | (m >= cm.IMAGE_END)]
    return set(np.unique(m).tolist())


def cover_index(spans):
    """(los, prefix max of ends) over the (va, bytes) spans: v is read when no span covers it."""
    iv = sorted((va, va + len(b)) for va, b in spans if b)
    los, maxhi, top = [], [], 0
    for lo, hi in iv:
        los.append(lo)
        top = max(top, hi)
        maxhi.append(top)
    return los, maxhi


def covered(index, v):
    los, maxhi = index
    i = bisect.bisect_right(los, v) - 1
    return i >= 0 and maxhi[i] > v


def gather(proc, G, gblob, ep, img_needs, class_needs):
    """capture_midplay's gather, plus the whole-object roots of the team tick (H, M, MI, MA,
    teams, 22 rows of each player table) and the typed class chains of every row."""
    spans = [(G, gblob)]
    roots = {}
    for (root, offs), need in img_needs.items():
        X = root[1]
        if X not in roots:
            roots[X] = proc.u64(X) or ep.img.get(X, 0)
            pg = X & ~(cm.PAGE - 1)
            spans.append((pg, proc.span(pg, cm.PAGE)))
        obj = cm.walk(proc, roots[X], offs, G, gblob)
        if obj and not cm.image(obj) and obj != G:
            spans.append((obj, proc.span(obj, max(need, DEEP_WIN))))
    for X in (H_GLOBAL, M_GLOBAL):
        pg = X & ~(cm.PAGE - 1)
        spans.append((pg, proc.span(pg, cm.PAGE)))
    mi = struct.unpack_from("<Q", gblob, cm.MI_OFF)[0]
    teams = [struct.unpack_from("<Q", gblob, cm.team_off(s))[0] for s in (0, 1)]
    rows = {c: [struct.unpack_from("<Q", gblob, cm.row_off(c, k))[0]
                for k in range(seed.PLAYER_COUNT)] for c in seed.PLAYER_TABLE}
    H = proc.u64(H_GLOBAL) or ep.img.get(H_GLOBAL, 0)
    M = proc.u64(M_GLOBAL) or ep.img.get(M_GLOBAL, 0)
    W = proc.u64(M + 0x118) if M else 0
    blk = proc.u64(W + 0x10) if W else 0
    named = [(mi, MI_WIN), (H, H_WIN), (M, M_WIN), (blk, cm.MA_WIN)]
    named += [(t, TEAM_WIN) for t in teams]
    named += [(v, ROW_WIN) for c in rows for v in rows[c]]
    for va, win in named:
        if va and not cm.image(va):
            spans.append((va, proc.span(va, win)))
    for c, recs in list(rows.items()) + [("team", teams)]:
        for (_, offs), need in class_needs.get(c, {}).items():
            if not offs:
                continue
            for r in recs:
                obj = cm.walk(proc, r, offs) if r else None
                if obj and not cm.image(obj):
                    spans.append((obj, proc.span(obj, max(need, DEEP_WIN))))
    return spans, {"H": H, "M": M, "MA": blk + 8 if blk else 0, "mi": mi, "teams": teams}


def expand(proc, spans, depth, stats):
    """Follow heap pointers from the newest spans, up to depth rounds. Each round reads one
    DEEP_WIN window per pointer not already inside a read span; spans are added in place."""
    fresh = list(spans)
    for _ in range(depth):
        index = cover_index(spans)
        cands = set()
        for _, buf in fresh:
            cands |= heap_ptrs(buf)
        new = []
        for v in sorted(cands):
            if covered(index, v):
                continue
            if stats["objs"] >= MAX_OBJS or stats["bytes"] >= MAX_BYTES:
                break
            b = proc.span(v, DEEP_WIN)
            stats["objs"] += 1
            if b:
                new.append((v, b))
                stats["bytes"] += len(b)
        if not new:
            break
        spans.extend(new)
        fresh = new
        stats["rounds"] += 1
    return spans


def capture(proc, G, ep, img_needs, class_needs, out_root, label, log, situation, frame_max, depth):
    """One capture. Kept only if MI's header and H's vtable are unchanged across the reads, the
    G slots naming MI and both teams are unchanged, and the MI frame counter advanced by at most
    frame_max."""
    mi = ep.g[cm.MI_OFF]
    tm = [ep.g[cm.team_off(0)], ep.g[cm.team_off(1)]]
    H = proc.u64(H_GLOBAL) or ep.img.get(H_GLOBAL, 0)
    hdr0 = (proc.read(mi, 8), proc.read(H, 8) if H else None)
    f0 = proc.i32(mi + 0x1198)
    pre = cm.read_state(proc, mi)
    r0, t0 = proc.reads, time.time()
    log("CAPTURE start %s situation=%s" % (label, situation))
    blob = proc.span(G, cm.G_SIZE)
    if len(blob) < cm.G_SIZE:
        log("REJECT %s: G blob unreadable" % label)
        return False
    gblob, patched = cm.patched_blob(blob, ep)
    spans, info = gather(proc, G, gblob, ep, img_needs, class_needs)
    stats = {"objs": 0, "bytes": 0, "rounds": 0}
    spans = expand(proc, spans, depth, stats)
    t1 = time.time()
    hdr1 = (proc.read(mi, 8), proc.read(H, 8) if H else None)
    f1 = proc.i32(mi + 0x1198)
    post = cm.read_state(proc, mi)
    # G's slots are published per match step and cleared between steps: each must read 0 or
    # still name the captured object.
    slots_same = all(proc.u64(G + off) in (0, want) for off, want in
                     [(cm.MI_OFF, mi), (cm.team_off(0), tm[0]), (cm.team_off(1), tm[1])])
    df = f1 - f0
    ok_hdr = (all(hdr0) and hdr0 == hdr1 and hdr1[1] == struct.pack("<Q", H_VTABLE))
    ok_frame = 0 <= df <= frame_max
    if not (ok_hdr and ok_frame and slots_same):
        log("REJECT %s headers_same=%s slots_same=%s frame_delta=%d objs=%d secs=%.4f" % (
            label, ok_hdr, slots_same, df, stats["objs"], t1 - t0))
        return False
    merged = cm.merge(spans)
    total = sum(len(b) for _, b in merged)
    meta = {"pid": proc.pid, "g": G, "mi": mi, "teams": tm, "h": H, "m": info["M"],
            "ma": info["MA"], "arg_win": {}, "instances": [],
            "patched_slots": ["0x%X" % o for o in patched], "label": label,
            "situation": situation, "pre": pre, "post": post,
            "frame_pre": f0, "frame_post": f1, "frame_delta": df, "headers_same": ok_hdr,
            "bfs": stats, "reads": proc.reads - r0, "secs_read": t1 - t0}
    out_dir = os.path.join(out_root, label)
    cm.write_capture(out_dir, meta, merged, gblob)
    cm.own(out_dir)
    log("ACCEPT %s bytes=%d maps=%d reads=%d objs=%d rounds=%d secs=%.4f frame_delta=%d patched=%d" % (
        label, total, len(merged), meta["reads"], stats["objs"], stats["rounds"], t1 - t0, df,
        len(patched)))
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--spec-dir", default=cm.DEFAULT_SPECS)
    ap.add_argument("--roots", default=cm.DEFAULT_ROOTS)
    ap.add_argument("--pid", type=int, default=0)
    ap.add_argument("--dur", type=float, default=900)
    ap.add_argument("--max-caps", type=int, default=4)
    ap.add_argument("--open-every", type=float, default=20)
    ap.add_argument("--frame-max", type=int, default=2)
    ap.add_argument("--depth", type=int, default=2)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    logf = open(os.path.join(a.out, "capture.log"), "a", buffering=1)
    cm.own(a.out)
    t_start = time.time()

    def log(msg):
        logf.write("%s +%.1fs %s\n" % (time.strftime("%H:%M:%S"), time.time() - t_start, msg))

    pid = a.pid or int(subprocess.check_output(["pgrep", "-x", "PES2021.exe"]).split()[0])
    proc = cm.Proc(pid)
    typed = json.load(open(a.roots)) if os.path.exists(a.roots) else {}
    img_needs = cm.collect_needs(a.spec_dir, typed, None)
    class_needs = {c: cm.collect_needs(a.spec_dir, typed, c) for c in ("team", "man", "anime", "slot")}
    first_hops = {offs[0] for (root, offs) in img_needs
                  if root == ("IMG", seed.GVA) and offs and offs[0] + 8 <= cm.G_SIZE}
    required = cm.required_offs(["team", "man", "anime", "slot"], seed.PLAYER_COUNT)
    img_vas = {root[1] for (root, _) in img_needs if root[1] != seed.GVA} | {H_GLOBAL, M_GLOBAL}
    ep = cm.Epoch(required, first_hops, img_vas)
    forced = os.path.join(a.out, "FORCE")
    log("start pid=%d img_chains=%d required_slots=%d frame_max=%d depth=%d max_caps=%d" % (
        pid, len(img_needs), len(required), a.frame_max, a.depth, a.max_caps))
    caps = tries = 0
    last_open = last_try = -1e9
    last_mi_t = -1e9
    frame_prev, frame_t = None, -1e9
    last_key = None
    t_end = time.time() + a.dur
    while time.time() < t_end and caps < a.max_caps:
        now = time.time()
        G = proc.u64(seed.GVA)
        if not G:
            time.sleep(cm.POLL)
            continue
        blob = proc.span(G, cm.G_SIZE)
        mi_b = struct.unpack_from("<Q", blob, cm.MI_OFF)[0] if len(blob) >= cm.G_SIZE else 0
        if mi_b:
            if now - last_mi_t > cm.MATCH_GAP:
                ep.clear()
                log("MATCH MI=0x%x G=0x%x" % (mi_b, G))
            last_mi_t = now
            ep.fill(proc, blob)
        elif ep.g and now - last_mi_t > cm.MATCH_GAP:
            log("MATCH END (no step window for %.1f s)" % cm.MATCH_GAP)
            ep.clear()
        mi = ep.g.get(cm.MI_OFF, 0)
        if not mi:
            time.sleep(cm.POLL)
            continue
        st = cm.read_state(proc, mi)
        if st["frame"] != frame_prev:
            frame_prev, frame_t = st["frame"], now
        key = (st["per"], st["play"], st["restart"], st["side"], st["reason"])
        if key != last_key:
            log("STATE per=%d play=%d restart=%d side=%d reason=%d frame=%d" % (
                st["per"], st["play"], st["restart"], st["side"], st["reason"], st["frame"]))
            last_key = key
        wanted = None
        if os.path.exists(forced):
            os.remove(forced)
            wanted = "forced"
        elif st["play"] == 1 and now - last_open >= a.open_every:
            wanted = "open"
        live = now - frame_t < cm.FRAME_LIVE and ep.ready()
        if wanted and live and now - last_try >= cm.RETRY:
            tries += 1
            last_try = now
            label = "f%02d-%s-p%d-play%d-fr%d" % (tries, wanted, st["per"], st["play"], st["frame"])
            if capture(proc, G, ep, img_needs, class_needs, a.out, label, log,
                       dict(st, trigger=wanted), a.frame_max, a.depth):
                caps += 1
                last_open = now
        time.sleep(cm.POLL)
    log("done caps=%d tries=%d reads=%d" % (caps, tries, proc.reads))


if __name__ == "__main__":
    sys.exit(main())
