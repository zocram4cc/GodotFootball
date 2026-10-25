#!/usr/bin/env python3
"""Live-state seed for impure harness specs (both sides).

The impure specs shadow G (the record table at image global 0x1436F3DC8)
and the objects G points to in synthetic regions, but every region started
zero-filled, so the functions' initialisation gates failed and they exited
before reaching the fields the cases vary (docs/re/pes-re-impure-state.md).

A seed file (PES-derived, build tree / scratch only, never committed)
records the REAL bytes of the shadowed objects. Two sources:

  live:       a live_snapshot.py dir (the running game, mid-match): the G
              object (g.bin) + every captured mapping; a region's bytes come
              from the object the pointer chain in its name resolves to
              (G+slot -> live child -> ...).
  emulated:   a snapshot.py dir (PES's own G ctor/init run in the emulator):
              real G bytes, child slots 0 (match load never ran) -> only the
              G-shadow is seeded.

Per spec, applied on BOTH the unicorn harness (run.py) and the translated
side (diffmain.py) BEFORE the per-case inputs:

  * the G-shadow region    <- the live/emulated G object bytes (0x2e58).
  * each IMG.* child region <- the live bytes of the object its name chain
    resolves to (live source only).
  * argument roots (A0/...) <- team record T[--team] (arg_roots, live only),
    its child chains walked from T; unbound roots stay zeroed.
  * pointer slots the spec re-points (regions_init) keep their SYNTHETIC
    bases: the spec linkage is written AFTER the seed, exactly as before.
  * live pointers the G-shadow holds to seeded children are re-pointed at
    the synthetic bases (patch_g_shadow), so the G-shadow is a faithful G
    whose child slots go into the harness's own regions.

Seed JSON (written next to the corpus by run.py):
  {"g_base": "0x...", "regions": [{"name", "base", "off", "size", "hex"}...]}

Bank JSON (--bank-classes): {"class": team|anime|man|slot|none, "instances": [seed JSON...]}.
Instance k binds A0 to the k-th live record of that class; run.py case n uses instance n % N.
"""
import json
import os
import struct

GVA = 0x1436F3DC8


def is_g(name):
    return name == "IMG.1436F3DC8" or name.split(".")[-1] == "1436F3DC8"


def load_live(dirpath):
    """(meta, {file_out: (entry, path)}) from a live_snapshot.py output dir."""
    meta = json.load(open(dirpath + "/live.json"))
    index = {}
    for e in meta["mappings"]:
        if "file_out" in e:
            index[e["file_out"]] = (e, dirpath + "/" + e["file_out"])
    return meta, index


def make_reader(index):
    """addr -> n bytes at live VA (None if not captured). Streams from disk."""
    import bisect
    spans = sorted((e["va"], e["hi"], path) for e, path in index.values())
    los = [s[0] for s in spans]

    def read(addr, n):
        i = bisect.bisect_right(los, addr) - 1
        if i < 0 or spans[i][1] <= addr:
            return None
        va, hi, path = spans[i]
        n = min(n, hi - addr)
        with open(path, "rb") as f:
            f.seek(addr - va)
            return f.read(n)
    return read


TEAM_SLOT = 0xA1  # team record s: T[s] = *(G + (s + TEAM_SLOT) * STRIDE)
STRIDE = 0x28


def chain_of(rname):
    """(root, offsets) a region shadows, or None. 'IMG.1436F3DC8.180' ->
    ('IMG', [0x180]) (G-rooted); 'A0.6D48.E0' -> ('A0', [0x6D48, 0xE0])
    (argument-rooted, bound by arg_roots)."""
    parts = rname.split(".")
    if parts[0] == "IMG":
        if int(parts[1], 16) != GVA:
            return None  # not a G slot (an image address or a shared sentinel)
        return "IMG", [int(p, 16) for p in parts[2:]]
    if parts[0][:1] == "A" and parts[0][1:].isdigit():
        return parts[0], [int(p, 16) for p in parts[1:]]
    return None


def walk(read, gbytes, g, start, offs):
    """Follow the pointer chain offs from start; None on a null hop or
    uncaptured memory. G's own slots come from g.bin: its live mapping is
    zero-filled in the snapshot (ArgState probe1)."""
    addr = start
    for off in offs:
        raw = gbytes[off:off + 8] if (addr == g and gbytes) else read(addr + off, 8)
        if not raw or len(raw) < 8:
            return None
        (v,) = struct.unpack("<Q", raw)
        if v == 0:
            return None
        addr = v
    return addr


def arg_roots(read, gbytes, g, team=0):
    """Argument-root bindings: A0 = team record T[team], walked from G."""
    t = walk(read, gbytes, g, g, [(team + TEAM_SLOT) * STRIDE])
    return {"A0": t} if t else {}


def region_window(spec, rname):
    """Seed window for one region: [0, max spec-field offset + slack). The
    spec's fields are the analyzer's static read/write set for the function,
    so the window covers every field it can touch."""
    top = 0
    for loc in list((spec.get("inputs") or {}).values()) + \
            list((spec.get("outputs") or {}).values()):
        if loc.get("region") == rname:
            top = max(top, int(loc["offset"], 16) + loc.get("size", 8))
    return (top + 0xFFF) & ~0xFFF if top else 0x1000


def build_live_seed(spec, meta, read, gbytes, roots=None, page=0x1000):
    """
    Returns ({region: [[off, bytes], ...]}, {region: live_addr}). The
    G-shadow and every IMG child whose pointer chain from G resolves to a
    captured mapping get real bytes; an argument root (A0/...) gets its
    object from roots (arg_roots), with its child chain walked the same way.
    Unbound roots and unresolved chains stay zeroed.

    The G-shadow gets the full G object (g.bin, 0x2e58): the function's
    readable set is not known a priori, and the object is small. A child
    region gets [0, window) where window covers the spec's fields for that
    region (the analyzer's static read/write set) plus slack.
    """
    regions = spec.get("regions") or {}
    roots = roots or {}
    out, live = {}, {}
    for rname in regions:
        if is_g(rname):
            if gbytes:
                out[rname] = [[0, gbytes]]
            continue
        chain = chain_of(rname)
        if chain is None:
            continue
        root, offs = chain
        start = meta["g"] if root == "IMG" else roots.get(root)
        if start is None:
            continue  # unbound argument root
        addr = walk(read, gbytes, meta["g"], start, offs)
        if addr is None or read(addr, 4) is None:
            continue  # null hop or uncaptured
        blob = read(addr, region_window(spec, rname) or page)
        if blob:
            out[rname] = [[0, blob]]
            live[rname] = addr
    return out, live


IMAGE_BASE, IMAGE_END = 0x140000000, 0x15B97C000  # pes_full.bin extent


def patch_live_ptrs(spec, seed, live, meta, read):
    """Make every seeded blob safe inside the harness, per 8-byte word:

      0 -> keep;  image VA (vtables, .rdata/.data) -> keep;
      the blob's own object address -> its synthetic region base;
      another seeded object's live address -> that object's region base
        (the spec's regions_init re-points the slots it lists; this covers
        the rest of the shadow to the same children);
      any other live-heap address -> 0 (harness maps only the spec's
        regions: the slot is dangling there, and 0 reproduces the
        zero-run's null-check; both sides get the same bytes, so the
        branch stays symmetric).

    read may be None (emulated source: no live heap exists, so the
    last branch is skipped)."""
    regions = spec.get("regions") or {}
    live2base = {addr: int(regions[r]["base"], 16)
                for r, addr in live.items() if r in regions}
    self2base = dict(live2base)
    gname = next((n for n in seed if is_g(n)), None)
    if gname:
        self2base[meta["g"]] = int(regions[gname]["base"], 16)
    for rname, chunks in seed.items():
        off, blob = chunks[0]
        b = bytearray(blob)
        for i in range(0, len(b) - 7, 8):
            (v,) = struct.unpack("<Q", b[i:i + 8])
            if v in live2base:
                b[i:i + 8] = struct.pack("<Q", live2base[v])
            elif v in self2base:
                b[i:i + 8] = struct.pack("<Q", self2base[v])
            elif read is not None and v and not (IMAGE_BASE <= v < IMAGE_END) \
                    and read(v, 1) is not None:
                b[i:i + 8] = struct.pack("<Q", 0)
        chunks[0][1] = bytes(b)


def artifact_of(spec, seed):
    """JSON the diff main and run.py consume: which regions are seeded, with
    base, offset and hex bytes. Both sides apply the identical bytes from
    this file (the driver passes it to run.py and the build)."""
    regions = spec.get("regions") or {}
    gname = next((n for n in regions if is_g(n)), None)
    a = {"g_base": ("0x%X" % int(regions[gname]["base"], 16)
                    if gname else None),
         "regions": []}
    for rname in sorted(seed):
        off, blob = seed[rname][0]
        a["regions"].append({"name": rname,
                             "base": "0x%X" % int(regions[rname]["base"], 16),
                             "off": off, "size": len(blob),
                             "hex": blob.hex()})
    return a


def seed_for_spec(spec, live_dir=None, emulated_dir=None, page=0x1000, team=None):
    """The per-spec seed artifact (dict) from a live dir or an emulated dir."""
    regions = spec.get("regions") or {}
    gname = next((n for n in regions if is_g(n)), None)
    if emulated_dir:
        # The emulated G (snapshot.py): real G bytes, child slots 0 (match
        # load never ran) -> only the G-shadow is seeded; the spec's
        # regions_init re-points its child slots to the synthetic regions.
        # meta carries the emulated g pointer for self-pointer patching.
        meta = json.load(open(emulated_dir + "/meta.json")) \
            if os.path.exists(emulated_dir + "/meta.json") else {"g": None}
        gbytes = open(emulated_dir + "/G.bin", "rb").read()
        seed = {gname: [[0, gbytes]]} if gname else {}
        patch_live_ptrs(spec, seed, {}, meta, None)
        return artifact_of(spec, seed)
    meta, index = load_live(live_dir)
    read = make_reader(index)
    gbytes = open(live_dir + "/g.bin", "rb").read() if meta.get("g_blob") else b""
    roots = arg_roots(read, gbytes, meta["g"], team) if team is not None else {}
    seed, live = build_live_seed(spec, meta, read, gbytes, roots, page)
    patch_live_ptrs(spec, seed, live, meta, read)
    return artifact_of(spec, seed)


# Argument-class banks (live only): case i runs instance i % N. Instances are
# team records T[s] (s = 0, 1), or the 22 on-pitch player records of one table
# (anime / man / slot: G-table entries k+0xA5 / k+0xBF / k+0xF3).
PLAYER_TABLE = {"anime": 0xA5, "man": 0xBF, "slot": 0xF3}
PLAYER_COUNT = 22  # on-pitch slot entries 0..21 (pes-re-player-record.md 1.1)


def instance_roots(read, gbytes, g, cls):
    """Candidate A0 addresses of argument class cls (null entries dropped)."""
    if cls == "team":
        cands = [walk(read, gbytes, g, g, [(s + TEAM_SLOT) * STRIDE]) for s in (0, 1)]
    elif cls in PLAYER_TABLE:
        cands = [walk(read, gbytes, g, g, [(k + PLAYER_TABLE[cls]) * STRIDE])
                 for k in range(PLAYER_COUNT)]
    else:
        return []
    return [c for c in cands if c]


def bank_for_spec(spec, live_dir, cls, page=0x1000):
    """Per-case instance bank (live only): entry k is the seed artifact with A0
    bound to instance k of class cls. With no live instance (or class none) the
    bank is the single G-only seed."""
    meta, index = load_live(live_dir)
    read = make_reader(index)
    gbytes = open(live_dir + "/g.bin", "rb").read() if meta.get("g_blob") else b""
    bank = []
    for a0 in instance_roots(read, gbytes, meta["g"], cls) or [None]:
        seed, live = build_live_seed(spec, meta, read, gbytes,
                                     {"A0": a0} if a0 else {}, page)
        patch_live_ptrs(spec, seed, live, meta, read)
        bank.append(artifact_of(spec, seed))
    return bank



if __name__ == "__main__":
    import os
    import sys
    ap = argparse = __import__("argparse")
    p = argparse.ArgumentParser()
    p.add_argument("--live", help="a live_snapshot.py output dir")
    p.add_argument("--emulated", help="a snapshot.py output dir")
    p.add_argument("--spec-dir", required=True)
    p.add_argument("--out", required=True, help="seed dir (build tree only)")
    p.add_argument("--page", type=lambda s: int(s, 0), default=0x1000)
    p.add_argument("--only", nargs="*", default=None)
    p.add_argument("--team", type=int, default=None,
                   help="bind the A0 argument root to team record T[N] (live only)")
    p.add_argument("--bank-classes", default=None,
                   help="JSON {spec name (lower-case): team|anime|man|slot|none}: write "
                        "<name>.bank.json (per-case instances, live only) instead of <name>.seed.json")
    args = p.parse_args()
    if not args.live and not args.emulated:
        p.error("one of --live / --emulated")
    os.makedirs(args.out, exist_ok=True)
    live_dir = args.live or None
    classes = json.load(open(args.bank_classes)) if args.bank_classes else None
    if classes is not None and live_dir is None:
        p.error("--bank-classes needs --live")
    for f in sorted(os.listdir(args.spec_dir)):
        if not f.endswith(".json"):
            continue
        if args.only and f[:-5].lower() not in {v.lower() for v in args.only}:
            continue
        spec = json.load(open(os.path.join(args.spec_dir, f)))
        if classes is not None:
            cls = classes.get(f[:-5].lower(), "none")
            bank = bank_for_spec(spec, live_dir, cls, args.page)
            json.dump({"class": cls, "instances": bank},
                      open(os.path.join(args.out, f[:-5] + ".bank.json"), "w"))
            print("%s: class %s, %d instances" % (f[:-5], cls, len(bank)))
            continue
        a = seed_for_spec(spec, live_dir, args.emulated, args.page, args.team)
        json.dump(a, open(os.path.join(args.out, f[:-5] + ".seed.json"), "w"))
        print("%s: %d regions seeded" % (f[:-5], len(a["regions"])))
