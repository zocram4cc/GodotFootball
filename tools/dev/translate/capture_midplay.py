#!/usr/bin/env python3
"""Mid-play capture of the impure harness's object graph, read-only.

Reads /proc/<pid>/mem with pread only: never writes, pauses, signals or
attaches. Logs every capture and every state change (the frame counter is not
logged per tick).

Waits for a live match: MI = *(G+0x270) non-null AND the frame counter
(MI+0x1198) advanced within the last 2 s (a stuck frame is a finished match or
a menu over one). The bot churns matches: between match steps MI and both
team-record pointers read null for ~0.1 s, so MI is probed at 200 Hz and the
snapshot is read into memory inside a step; files are written only after the
post-check. Triggers: match start (MI seen again after a >2 s gap), each
period start, open play every --open-every s, and the first stoppage of each
restart kind (MI+0x1554) per period. A capture is kept only if MI is non-null
and the same object after the reads, and G and both team records are
unchanged; otherwise it is dropped and the rejection is logged.

Captured, and nothing else: G (0x2E58 bytes); each image global the impure
specs root at (IMG.<X>, one page each) and its chain objects; the on-pitch
records (2 teams, 22 anime, 22 man, 22 slot: G rows), MI and MA, each with a
window the specs need; and every A<n> chain node the impure specs name, with
the window the spec's fields need. A whole-process snapshot (live_snapshot.py)
is about 3.8 GB; a capture here is about 26 MB.

Output is live_snapshot.py's format (live.json + <va>-<hi>.bin + g.bin), so
seed.py's load_live/make_reader read it unchanged.

Usage: sudo -n python3 capture_midplay.py --out DIR [--spec-dir D] [--dur S]
       [--max-caps N] [--open-every S] [--settle S] [--pid P]
PES-derived scratch data: never committed.
"""
import argparse
import json
import os
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import seed  # noqa: E402  GVA, TEAM_SLOT, STRIDE, PLAYER_TABLE, PLAYER_COUNT, region_window

MI_OFF = 0x270            # G+0x270 = MI (match info)
MI_WIN = 0x4000           # MI fields read up to 0x1900 (live_session.py)
MA_WIN = 0x26000          # MA+0x24D54 (live_session.py T6a)
G_SIZE = 0x2E58           # G object size (seed.py)
PAGE = 0x1000
POLL = 0.005              # s; MI non-null windows are ~0.1 s
MATCH_GAP = 2.0           # s without MI before a new match is announced
FRAME_LIVE = 2.0          # s: the frame must have advanced this recently
IMAGE_BASE, IMAGE_END = 0x140000000, 0x15B97C000
H_GLOBAL, M_GLOBAL = 0x1436F3F98, 0x1436F3F78   # live_session.py anchors (MA chain)
DEFAULT_SPECS = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "..", "diffharness", "specs", "impure")


def image(va):
    return IMAGE_BASE <= va < IMAGE_END


class Proc:
    """Read-only view of /proc/<pid>/mem."""

    def __init__(self, pid):
        self.pid = pid
        self.fd = os.open("/proc/%d/mem" % pid, os.O_RDONLY)
        self.reads = 0

    def read(self, va, n):
        self.reads += 1
        if not 0 < va < (1 << 47):  # a garbage pointer walked from a non-pointer slot
            return None
        try:
            b = os.pread(self.fd, n, va)
        except (OSError, OverflowError):
            return None
        return b if len(b) == n else None

    def u64(self, va):
        b = self.read(va, 8)
        return struct.unpack("<Q", b)[0] if b else 0

    def i32(self, va):
        b = self.read(va, 4)
        return struct.unpack("<i", b)[0] if b else 0

    def span(self, va, want):
        """Bytes from va, up to want: one pread if the whole range is mapped,
        else page by page up to the first unreadable page."""
        b = self.read(va, want)
        if b is not None:
            return b
        out = bytearray()
        cur, end = va, va + want
        while cur < end:
            n = min(PAGE - cur % PAGE, end - cur)
            chunk = self.read(cur, n)
            if chunk is None:
                break
            out += chunk
            cur += n
        return bytes(out)


def walk(proc, start, offs):
    """Follow offs from start (live reads). 0 on a null hop or unreadable memory."""
    addr = start
    for off in offs:
        if not addr:
            return 0
        addr = proc.u64(addr + off)
    return addr


def collect_needs(spec_dir):
    """Window per chain node the impure specs walk. Key (root, offs-prefix):
    root ('IMG', X) for IMG.<X>.<offs> (X = image global; GVA = G), None for
    A<n>.<offs> (an argument root, bound to any on-pitch record). Value: bytes
    the node needs (its next pointer, or the region's fields), page-rounded."""
    needs = {}
    for f in sorted(os.listdir(spec_dir)):
        if not f.endswith(".json"):
            continue
        spec = json.load(open(os.path.join(spec_dir, f)))
        for rname in spec.get("regions") or {}:
            parts = rname.split(".")
            try:
                if parts[0] == "IMG":
                    root, offs = ("IMG", int(parts[1], 16)), tuple(int(p, 16) for p in parts[2:])
                elif parts[0][:1] == "A" and parts[0][1:].isdigit():
                    root, offs = None, tuple(int(p, 16) for p in parts[1:])
                else:
                    continue
            except ValueError:
                continue  # a non-offset suffix (A0.6D50.pad): zero-filled room, not a chain
            w = seed.region_window(spec, rname)
            for i in range(len(offs) + 1):
                need = offs[i] + 8 if i < len(offs) else w
                key = (root, offs[:i])
                needs[key] = max(needs.get(key, 0), need)
    return {k: (v + PAGE - 1) & ~(PAGE - 1) for k, v in needs.items()}


def anchors(proc):
    """(G, MI, [T0, T1]) read live. MI and the teams are 0 between match steps."""
    G = proc.u64(seed.GVA)
    if not G:
        return 0, 0, [0, 0]
    mi = proc.u64(G + MI_OFF)
    teams = [proc.u64(G + (s + seed.TEAM_SLOT) * seed.STRIDE) for s in (0, 1)]
    return G, mi, teams


def read_state(proc, mi):
    return {"per": proc.i32(mi + 0x1168), "play": proc.i32(mi + 0x116C),
            "restart": proc.i32(mi + 0x1554), "side": proc.i32(mi + 0x1558),
            "reason": proc.i32(mi + 0x1574), "frame": proc.i32(mi + 0x1198)}


def instances(proc, G):
    """(class, va) of every on-pitch record: 2 teams, then 22 each of anime/man/slot."""
    out = [("team", proc.u64(G + (s + seed.TEAM_SLOT) * seed.STRIDE)) for s in (0, 1)]
    for cls, base in seed.PLAYER_TABLE.items():
        out += [(cls, proc.u64(G + (k + base) * seed.STRIDE)) for k in range(seed.PLAYER_COUNT)]
    return [(c, v) for c, v in out if v]


def merge(spans):
    """Union of (va, bytes) spans as non-overlapping (lo, bytearray). Overlap keeps
    the first read's bytes (one snapshot, ms skew)."""
    out = []  # [lo, hi, buf]
    for va, data in sorted(spans, key=lambda s: s[0]):
        if not data:
            continue
        hi = va + len(data)
        if out and va <= out[-1][1]:
            cur = out[-1]
            if hi > cur[1]:
                cur[2] += data[cur[1] - va:]
                cur[1] = hi
        else:
            out.append([va, hi, bytearray(data)])
    return [(lo, buf) for lo, _, buf in out]


def read_snapshot(proc, needs):
    """Read the capture into memory. Returns (G, mi, teams, merged spans, insts, blk)."""
    G, mi, teams = anchors(proc)
    gblob = proc.span(G, G_SIZE) if G else b""  # first: the torn check reads it
    arg_win = max([v for (r, _), v in needs.items() if r is None] or [PAGE])
    spans = []
    # image globals and the chain objects under them
    roots = {}
    for (root, offs), need in needs.items():
        if root is None:
            continue
        X = root[1]
        if X not in roots:
            roots[X] = proc.u64(X)
            pg = X & ~(PAGE - 1)
            spans.append((pg, proc.span(pg, PAGE)))
        obj = walk(proc, roots[X], offs)
        if obj and not image(obj):
            want = max(need, G_SIZE) if (X == seed.GVA and not offs) else need
            spans.append((obj, proc.span(obj, want)))
    # on-pitch records (window arg_win), then their argument chain nodes
    insts = instances(proc, G)
    for _, inst in insts:
        spans.append((inst, proc.span(inst, arg_win)))
        for (root, offs), need in needs.items():
            if root is None and offs:
                obj = walk(proc, inst, offs)
                if obj and not image(obj):
                    spans.append((obj, proc.span(obj, need)))
    if mi:
        spans.append((mi, proc.span(mi, MI_WIN)))
    # MA = blk + 8, blk = *(*(M+0x118)+0x10)
    M = proc.u64(M_GLOBAL)
    W = proc.u64(M + 0x118) if M else 0
    blk = proc.u64(W + 0x10) if W else 0
    if blk:
        spans.append((blk, proc.span(blk, MA_WIN)))
    for X in (H_GLOBAL, M_GLOBAL):
        pg = X & ~(PAGE - 1)
        spans.append((pg, proc.span(pg, PAGE)))
    return G, mi, teams, merge(spans), insts, blk, gblob, arg_win


def write_capture(out_dir, meta, merged, gblob):
    """Write one accepted capture: g.bin, one <va>-<hi>.bin per span, live.json."""
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "g.bin"), "wb") as w:
        w.write(gblob)
    maps, total = [], 0
    for lo, buf in merged:
        name = "%x-%x.bin" % (lo, lo + len(buf))
        with open(os.path.join(out_dir, name), "wb") as w:
            w.write(buf)
        maps.append({"va": lo, "hi": lo + len(buf), "file_out": name, "perms": "r--p", "file": ""})
        total += len(buf)
    meta.update({"g_blob": bool(gblob), "total_bytes": total, "n_mappings": len(maps),
                 "n_unreadable": 0, "mappings": maps})
    with open(os.path.join(out_dir, "live.json"), "w") as w:
        json.dump(meta, w)


def own(root):
    """Hand scratch files to the invoking user when run under sudo."""
    uid, gid = os.environ.get("SUDO_UID"), os.environ.get("SUDO_GID")
    if not (uid and gid):
        return
    for d, _, files in os.walk(root):
        for f in files + [""]:
            os.chown(os.path.join(d, f), int(uid), int(gid))


def capture(proc, needs, out_root, label, log, situation):
    """One capture. Kept only if MI is non-null and the same object after the reads,
    and G and both team records are unchanged."""
    G0, mi0, teams0 = anchors(proc)
    if not mi0 or not all(teams0):
        log("REJECT %s pre: MI=0x%x teams=%s" % (label, mi0, [hex(t) for t in teams0]))
        return False
    pre = read_state(proc, mi0)
    r0, t0 = proc.reads, time.time()
    log("CAPTURE start %s situation=%s" % (label, situation))
    G, mi, teams, merged, insts, blk, gblob, arg_win = read_snapshot(proc, needs)
    blob_ok = len(gblob) >= G_SIZE and struct.unpack_from("<Q", gblob, MI_OFF)[0] and all(
        struct.unpack_from("<Q", gblob, (s + seed.TEAM_SLOT) * seed.STRIDE)[0] for s in (0, 1))
    if not blob_ok:  # the G blob was read inside a match gap: torn, not a mid-play state
        log("REJECT %s torn: G blob has null MI or team slots" % label)
        return False
    t1 = time.time()
    G1, mi1, teams1 = anchors(proc)
    post = read_state(proc, mi1) if mi1 else {}
    if not (G1 == G0 and mi1 == mi0 and all(teams1) and teams1 == teams0):
        log("REJECT %s post: MI 0x%x->0x%x G 0x%x->0x%x teams=%s secs=%.3f" % (
            label, mi0, mi1, G0, G1, [hex(t) for t in teams1], t1 - t0))
        return False
    meta = {"pid": proc.pid, "g": G, "mi": mi, "teams": teams, "ma": (blk + 8) if blk else 0,
            "arg_win": arg_win, "instances": [{"cls": c, "va": v} for c, v in insts],
            "label": label, "situation": situation, "pre": pre, "post": post,
            "reads": proc.reads - r0, "secs_read": t1 - t0}
    write_capture(os.path.join(out_root, label), meta, merged, gblob)
    own(os.path.join(out_root, label))
    log("ACCEPT %s bytes=%d maps=%d reads=%d secs=%.3f inst=%d pre=%s post=%s" % (
        label, meta["total_bytes"], meta["n_mappings"], meta["reads"],
        time.time() - t0, len(insts), pre, post))
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--spec-dir", default=DEFAULT_SPECS)
    ap.add_argument("--pid", type=int, default=0)
    ap.add_argument("--dur", type=float, default=14400, help="seconds before stopping")
    ap.add_argument("--max-caps", type=int, default=40)
    ap.add_argument("--open-every", type=float, default=15, help="open-play capture interval, s")
    ap.add_argument("--settle", type=float, default=3, help="wait after a match start, s")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    logf = open(os.path.join(a.out, "capture.log"), "a", buffering=1)
    own(a.out)
    t_start = time.time()

    def log(msg):
        logf.write("%s +%.1fs %s\n" % (time.strftime("%H:%M:%S"), time.time() - t_start, msg))

    pid = a.pid or int(subprocess.check_output(["pgrep", "-x", "PES2021.exe"]).split()[0])
    proc = Proc(pid)
    needs = collect_needs(a.spec_dir)
    log("start pid=%d needs=%d spec_dir=%s max_caps=%d" % (pid, len(needs), a.spec_dir, a.max_caps))
    caps = tries = 0
    seen = set()            # (period, restart kind) captured
    last_open = last_try = -1e9
    last_mi_t = -1e9        # last time MI was non-null
    frame_prev, frame_t = None, -1e9   # last frame value and when it last changed
    prev_per, per_t, per_cap = None, 0.0, None
    match_t0, start_done = 0.0, False
    last_key, hb = None, 0.0
    t_end = time.time() + a.dur
    while time.time() < t_end and caps < a.max_caps:
        now = time.time()
        G, mi, _ = anchors(proc)
        if now - hb >= 60:
            log("heartbeat reads=%d MI=0x%x caps=%d tries=%d" % (proc.reads, mi, caps, tries))
            hb = now
        if not mi:
            time.sleep(POLL)
            continue
        if now - last_mi_t > MATCH_GAP:
            log("MATCH MI=0x%x G=0x%x" % (mi, G))
            match_t0, start_done, prev_per, per_cap = now, False, None, None
        last_mi_t = now
        st = read_state(proc, mi)
        if st["frame"] != frame_prev:
            frame_prev, frame_t = st["frame"], now
        key = (st["per"], st["play"], st["restart"], st["side"], st["reason"])
        if key != last_key:
            log("STATE per=%d play=%d restart=%d side=%d reason=%d frame=%d" % (
                st["per"], st["play"], st["restart"], st["side"], st["reason"], st["frame"]))
            last_key = key
        if st["per"] != prev_per:
            per_t, prev_per = now, st["per"]
        live = now - frame_t < FRAME_LIVE
        trig = None
        if not live:
            pass
        elif not start_done and now - match_t0 >= a.settle:
            trig, start_done, per_cap = "start", True, st["per"]
        elif per_cap is not None and st["per"] != per_cap and now - per_t >= a.settle:
            trig, per_cap = "period%d" % st["per"], st["per"]
        elif st["play"] == 1 and now - last_open >= a.open_every:
            trig = "open"
        elif st["play"] in (2, 3) and (st["per"], st["restart"]) not in seen:
            trig = "restart%d" % st["restart"]
        if trig and now - last_try >= 0.5:
            tries += 1
            last_try = now
            if trig == "open":
                last_open = now
            label = "t%03d-%s-p%d-f%d" % (tries, trig, st["per"], st["frame"])
            if capture(proc, needs, a.out, label, log, dict(st, trigger=trig)):
                caps += 1
                if trig.startswith("restart"):
                    seen.add((st["per"], st["restart"]))
        time.sleep(POLL)
    log("done caps=%d tries=%d reads=%d" % (caps, tries, proc.reads))


if __name__ == "__main__":
    sys.exit(main())
