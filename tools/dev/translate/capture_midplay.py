#!/usr/bin/env python3
"""Mid-play capture of the impure harness's object graph, read-only.

Reads /proc/<pid>/mem with pread only: never writes, pauses, signals or attaches.

Model (owner's live findings): MI = *(G+0x270) and the other G slots are published per
match step and cleared between steps; the objects they point to persist on the heap at
the same addresses across steps and matches. So the watcher:
  1. remembers each G slot seen non-zero in a step window (MI, both teams, the first
     --rows rows of each typed argument class, and the first hops of the G-rooted chains),
     and each image global the specs root at, for one match epoch (cleared after
     MATCH_GAP s with no step window);
  2. reads the objects at those addresses: the typed argument records, MI, MA, the
     G-rooted and image-rooted chains every spec walks, and only the argument chains of
     the roots typed with each class (impure_roots.json). A zero G slot in the blob is
     patched from the epoch; the patched slots are listed in live.json;
  3. keeps a capture only if the vtable (first 8 bytes) of MI and both teams is the same
     before and after the reads, and the MI frame counter advanced by at most --frame-max.

Triggers, while the game is live (the frame counter advanced within the last 2 s):
  - a stoppage: play enters 2 or 3 from open play. One capture per stoppage event,
    retried until one passes the gates; dropped if play is open again first;
  - open play: one capture per --open-every s while play is open, retried until one passes;
  - a FORCE file in --out: one capture of the current state, then the file is removed.

Output is live_snapshot.py's format (live.json + <va>-<hi>.bin + g.bin), so seed.py's
load_live/make_reader read it unchanged. Captures are PES-derived scratch data: never
committed.

Usage: sudo -n python3 capture_midplay.py --out DIR [--spec-dir D] [--roots F] [--rows N]
       [--dur S] [--max-caps N] [--open-every S] [--frame-max N] [--pid P]
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

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SPECS = os.path.join(HERE, "..", "diffharness", "specs", "impure")
DEFAULT_ROOTS = os.path.join(HERE, "impure_roots.json")
MI_OFF = 0x270            # G+0x270 = MI (match info)
MI_WIN = 0x4000           # MI fields read up to 0x1900 (live_session.py)
MA_WIN = 0x26000          # MA+0x24D54 (live_session.py T6a)
G_SIZE = 0x2E58           # G object size (seed.py)
PAGE = 0x1000
POLL = 0.005              # s
MATCH_GAP = 2.0           # s without a step window: the match epoch ends
FRAME_LIVE = 2.0          # s: the frame must have advanced this recently
RETRY = 0.3               # s between attempts for one wanted capture
IMAGE_BASE, IMAGE_END = 0x140000000, 0x15B97C000
H_GLOBAL, M_GLOBAL = 0x1436F3F98, 0x1436F3F78   # live_session.py anchors (MA chain)


def image(va):
    return IMAGE_BASE <= va < IMAGE_END


def team_off(s):
    return (s + seed.TEAM_SLOT) * seed.STRIDE


def row_off(cls, k):
    return (k + seed.PLAYER_TABLE[cls]) * seed.STRIDE


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
        """Bytes from va, up to want: one pread if the whole range is mapped, else page by
        page up to the first unreadable page."""
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


def walk(proc, start, offs, G=0, gblob=b""):
    """Follow offs from start. A hop read from G uses the (patched) G blob; every other hop
    reads live memory. 0 on a null hop or unreadable memory."""
    addr = start
    for off in offs:
        if not addr:
            return 0
        if addr == G and gblob:
            addr = struct.unpack_from("<Q", gblob, off)[0] if off + 8 <= len(gblob) else 0
        else:
            addr = proc.u64(addr + off)
    return addr


def collect_needs(spec_dir, typed, cls=None):
    """Window per chain node the impure specs walk, keyed (root, offs-prefix). cls None: the
    image-global chains (IMG.<X>.<offs>) of every spec, root ('IMG', X). cls <class>: the
    argument chains (A<n>.<offs>) of the roots typed <class> in typed ({spec: {root:
    class}}), root None: any instance of the class can be the root. Value: the bytes the
    node needs (its next hop, or the region's fields), page-rounded."""
    needs = {}
    for f in sorted(os.listdir(spec_dir)):
        if not f.endswith(".json"):
            continue
        s = f[:-5]
        spec = json.load(open(os.path.join(spec_dir, f)))
        for rname in spec.get("regions") or {}:
            parts = rname.split(".")
            try:
                if parts[0] == "IMG":
                    if cls is not None:
                        continue
                    root, offs = ("IMG", int(parts[1], 16)), tuple(int(p, 16) for p in parts[2:])
                elif parts[0][:1] == "A" and parts[0][1:].isdigit():
                    if cls is None or typed.get(s, {}).get(parts[0]) != cls:
                        continue
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


def classes_of(typed):
    return sorted({c for roots in typed.values() for c in roots.values()})


def required_offs(classes, rows):
    """G slots that must be non-zero in the epoch before a capture: MI, both teams, and the
    first rows of each typed class."""
    offs = [MI_OFF, team_off(0), team_off(1)]
    for c in classes:
        if c in seed.PLAYER_TABLE:
            offs += [row_off(c, k) for k in range(min(seed.PLAYER_COUNT, rows))]
    return sorted(set(offs))


def read_state(proc, mi):
    return {"per": proc.i32(mi + 0x1168), "play": proc.i32(mi + 0x116C),
            "restart": proc.i32(mi + 0x1554), "side": proc.i32(mi + 0x1558),
            "reason": proc.i32(mi + 0x1574), "frame": proc.i32(mi + 0x1198)}


class Epoch:
    """G slots and image globals seen non-zero in step windows, for one match. Every slot in
    `required` must have been seen before a capture; `extra` (first hops of G chains) is
    remembered when seen."""

    def __init__(self, required, extra, img_vas):
        self.required = sorted(set(required))
        self.g_offs = sorted(set(required) | set(extra))
        self.img_vas = sorted(set(img_vas))
        self.g, self.img = {}, {}

    def clear(self):
        self.g, self.img = {}, {}

    def fill(self, proc, blob):
        for off in self.g_offs:
            if off + 8 > len(blob):  # a chain past G's end (IMG.1436F3DC8.E1EE): no slot
                continue
            v = struct.unpack_from("<Q", blob, off)[0]
            if v:
                self.g[off] = v
        for X in self.img_vas:
            v = proc.u64(X)
            if v:
                self.img[X] = v

    def ready(self):
        return all(o in self.g for o in self.required)


def patched_blob(blob, ep):
    """The G blob with each zero slot the epoch remembers filled in. Returns (bytes, offsets)."""
    b = bytearray(blob)
    patched = []
    for off, v in sorted(ep.g.items()):
        if off + 8 <= len(b) and not any(b[off:off + 8]):
            b[off:off + 8] = struct.pack("<Q", v)
            patched.append(off)
    return bytes(b), patched


def merge(spans):
    """Union of (va, bytes) spans as non-overlapping (lo, bytearray). Overlap keeps the first
    read's bytes (one snapshot)."""
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


def gather(proc, G, gblob, ep, img_needs, class_needs, classes, rows):
    """The capture's objects: image-rooted chains, the typed argument records and their
    chains, MI, MA. Returns (spans, [(class, va)], MA, {class: record window})."""
    spans, roots = [], {}
    for (root, offs), need in img_needs.items():
        X = root[1]
        if X not in roots:
            roots[X] = proc.u64(X) or ep.img.get(X, 0)
            pg = X & ~(PAGE - 1)
            spans.append((pg, proc.span(pg, PAGE)))
        obj = walk(proc, roots[X], offs, G, gblob)
        if obj and not image(obj) and obj != G:  # G itself is g.bin
            spans.append((obj, proc.span(obj, need)))
    insts = [("team", struct.unpack_from("<Q", gblob, team_off(s))[0]) for s in (0, 1)]
    for c in classes:
        if c in seed.PLAYER_TABLE:
            insts += [(c, struct.unpack_from("<Q", gblob, row_off(c, k))[0])
                      for k in range(min(seed.PLAYER_COUNT, rows))]
    insts = [(c, v) for c, v in insts if v]
    win = {}
    for c, inst in insts:
        cn = class_needs.get(c, {})
        w = cn.get((None, ()), PAGE)
        win.setdefault(c, w)
        spans.append((inst, proc.span(inst, w)))
        for (_, offs), need in cn.items():
            if offs:
                obj = walk(proc, inst, offs)
                if obj and not image(obj):
                    spans.append((obj, proc.span(obj, need)))
    mi = struct.unpack_from("<Q", gblob, MI_OFF)[0]
    spans.append((mi, proc.span(mi, MI_WIN)))
    M = proc.u64(M_GLOBAL) or ep.img.get(M_GLOBAL, 0)
    W = proc.u64(M + 0x118) if M else 0
    blk = proc.u64(W + 0x10) if W else 0
    if blk:
        spans.append((blk, proc.span(blk, MA_WIN)))
    for X in (H_GLOBAL, M_GLOBAL):
        pg = X & ~(PAGE - 1)
        spans.append((pg, proc.span(pg, PAGE)))
    return spans, insts, (blk + 8) if blk else 0, win


def write_capture(out_dir, meta, merged, gblob):
    """One accepted capture: g.bin, one <va>-<hi>.bin per span, live.json."""
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
    meta.update({"g_blob": True, "total_bytes": total, "n_mappings": len(maps),
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


def capture(proc, G, ep, img_needs, class_needs, classes, rows, out_root, label, log,
            situation, frame_max):
    """One capture. Kept only if the vtable of MI and both teams is unchanged across the
    reads and the MI frame counter advanced by at most frame_max."""
    mi = ep.g[MI_OFF]
    tm = [ep.g[team_off(0)], ep.g[team_off(1)]]
    hdr0 = [proc.read(a, 8) for a in [mi] + tm]
    f0 = proc.i32(mi + 0x1198)
    pre = read_state(proc, mi)
    r0, t0 = proc.reads, time.time()
    log("CAPTURE start %s situation=%s" % (label, situation))
    blob = proc.span(G, G_SIZE)
    if len(blob) < G_SIZE:
        log("REJECT %s: G blob unreadable" % label)
        return False
    gblob, patched = patched_blob(blob, ep)
    spans, insts, ma, win = gather(proc, G, gblob, ep, img_needs, class_needs, classes, rows)
    t1 = time.time()
    hdr1 = [proc.read(a, 8) for a in [mi] + tm]
    f1 = proc.i32(mi + 0x1198)
    post = read_state(proc, mi)
    df = f1 - f0
    ok_hdr = all(hdr0) and hdr0 == hdr1
    ok_frame = 0 <= df <= frame_max
    if not (ok_hdr and ok_frame):
        log("REJECT %s headers_same=%s frame_delta=%d secs=%.4f" % (label, ok_hdr, df, t1 - t0))
        return False
    meta = {"pid": proc.pid, "g": G, "mi": mi, "teams": tm, "ma": ma, "arg_win": win,
            "instances": [{"cls": c, "va": v} for c, v in insts],
            "patched_slots": ["0x%X" % o for o in patched], "label": label,
            "situation": situation, "pre": pre, "post": post,
            "frame_pre": f0, "frame_post": f1, "frame_delta": df, "headers_same": ok_hdr,
            "reads": proc.reads - r0, "secs_read": t1 - t0}
    out_dir = os.path.join(out_root, label)
    write_capture(out_dir, meta, merge(spans), gblob)
    own(out_dir)
    log("ACCEPT %s bytes=%d maps=%d reads=%d secs=%.4f frame_delta=%d patched=%d inst=%d pre=%s" % (
        label, meta["total_bytes"], meta["n_mappings"], meta["reads"], t1 - t0, df,
        len(patched), len(insts), pre))
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--spec-dir", default=DEFAULT_SPECS)
    ap.add_argument("--roots", default=DEFAULT_ROOTS, help="impure_roots.json (typed classes)")
    ap.add_argument("--rows", type=int, default=4,
                    help="on-pitch rows captured per typed class (a bank of cap/N instances draws rows 0..)")
    ap.add_argument("--pid", type=int, default=0)
    ap.add_argument("--dur", type=float, default=900, help="seconds before stopping")
    ap.add_argument("--max-caps", type=int, default=14)
    ap.add_argument("--open-every", type=float, default=30, help="open-play capture interval, s")
    ap.add_argument("--frame-max", type=int, default=2, help="max MI frame advance in one capture")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    logf = open(os.path.join(a.out, "capture.log"), "a", buffering=1)
    own(a.out)
    t_start = time.time()

    def log(msg):
        logf.write("%s +%.1fs %s\n" % (time.strftime("%H:%M:%S"), time.time() - t_start, msg))

    pid = a.pid or int(subprocess.check_output(["pgrep", "-x", "PES2021.exe"]).split()[0])
    proc = Proc(pid)
    typed = json.load(open(a.roots)) if os.path.exists(a.roots) else {}
    classes = classes_of(typed)
    img_needs = collect_needs(a.spec_dir, typed, None)
    class_needs = {c: collect_needs(a.spec_dir, typed, c) for c in classes}
    first_hops = {offs[0] for (root, offs) in img_needs
                  if root == ("IMG", seed.GVA) and offs and offs[0] + 8 <= G_SIZE}
    required = required_offs(classes, a.rows)
    img_vas = {root[1] for (root, _) in img_needs if root[1] != seed.GVA} | {H_GLOBAL, M_GLOBAL}
    ep = Epoch(required, first_hops, img_vas)
    forced = os.path.join(a.out, "FORCE")
    log("start pid=%d img_chains=%d classes=%s rows=%d required_slots=%d frame_max=%d max_caps=%d" % (
        pid, len(img_needs), classes, a.rows, len(required), a.frame_max, a.max_caps))
    caps = tries = 0
    last_open = last_try = -1e9
    last_mi_t = -1e9
    frame_prev, frame_t = None, -1e9
    prev_play, wanted, last_key, hb = None, None, None, 0.0
    t_end = time.time() + a.dur
    while time.time() < t_end and caps < a.max_caps:
        now = time.time()
        G = proc.u64(seed.GVA)
        if not G:
            time.sleep(POLL)
            continue
        blob = proc.span(G, G_SIZE)
        mi_b = struct.unpack_from("<Q", blob, MI_OFF)[0] if len(blob) >= G_SIZE else 0
        if mi_b:
            if now - last_mi_t > MATCH_GAP:
                ep.clear()
                prev_play, wanted = None, None
                log("MATCH MI=0x%x G=0x%x" % (mi_b, G))
            last_mi_t = now
            ep.fill(proc, blob)
        elif ep.g and now - last_mi_t > MATCH_GAP:
            log("MATCH END (no step window for %.1f s)" % MATCH_GAP)
            ep.clear()
        if now - hb >= 60:
            log("heartbeat reads=%d epoch_slots=%d caps=%d tries=%d" % (
                proc.reads, len(ep.g), caps, tries))
            hb = now
        mi = ep.g.get(MI_OFF, 0)
        if not mi:
            time.sleep(POLL)
            continue
        st = read_state(proc, mi)
        if st["frame"] != frame_prev:
            frame_prev, frame_t = st["frame"], now
        key = (st["per"], st["play"], st["restart"], st["side"], st["reason"])
        if key != last_key:
            log("STATE per=%d play=%d restart=%d side=%d reason=%d frame=%d" % (
                st["per"], st["play"], st["restart"], st["side"], st["reason"], st["frame"]))
            last_key = key
        if st["play"] in (2, 3) and prev_play in (None, 1):
            wanted = "stop%d" % st["restart"]
        if os.path.exists(forced):
            os.remove(forced)
            wanted = "forced"
        if wanted is None and st["play"] == 1 and now - last_open >= a.open_every:
            wanted = "open"
        if wanted == "open" and st["play"] != 1:
            wanted = None
        if wanted and wanted.startswith("stop") and st["play"] == 1:
            log("DROP %s: play is open again before a capture passed" % wanted)
            wanted = None
        prev_play = st["play"]
        live = now - frame_t < FRAME_LIVE and ep.ready()
        if wanted and live and now - last_try >= RETRY:
            tries += 1
            last_try = now
            label = "t%03d-%s-p%d-f%d" % (tries, wanted, st["per"], st["frame"])
            if capture(proc, G, ep, img_needs, class_needs, classes, a.rows, a.out, label, log,
                       dict(st, trigger=wanted), a.frame_max):
                caps += 1
                if wanted == "open":
                    last_open = now
                wanted = None
        time.sleep(POLL)
    log("done caps=%d tries=%d reads=%d" % (caps, tries, proc.reads))


if __name__ == "__main__":
    sys.exit(main())
