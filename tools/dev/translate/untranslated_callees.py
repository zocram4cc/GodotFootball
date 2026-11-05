#!/usr/bin/env python3
"""untranslated_callees.py: the untranslated callees reachable from the given translated functions, with call sites.

The layer (src/pes/translated, or --layer DIR) holds one pes_fn_<VA>.cpp per translated function. A call to another
translated function is `pes_fn_<VA>(s)`. A call the translator could not link is the marker
`/*OUT-OF-SET CALL 0x<T>*/ pes_dispatch_missing(0x<T>);`, which aborts at run time (PES_DISPATCH_UNTRANSLATED) unless a
recovery or a GF_PES_SURVEY run catches it. This walks the calls from each root and lists every marker reached, with its
call sites: the calling function's VA and the 1-based line in its file (the layer carries no instruction addresses).

  untranslated_callees.py [--layer DIR] VA...
  untranslated_callees.py [--layer DIR] --hooks [--image IMG] [--first N] [--every N] [--prefix NAME] VA...

--hooks also prints, for each listed target, its hook_plan.py TARGETS row and, when hook_plan's prologue check passes,
the hooks.cfg line `hook NAME VA LEN FIRST EVERY BYTES` (rendezvous/rv.c). The check: the target is the .pdata entry of
its function, and rv_hooks.displaced_window covers 14 bytes with whole, branch-free, rip-free instructions. IMG is the
flat unpacked image (default $PES_EXE). Exit status: 2 if a root is not translated, 1 if --hooks refuses a target, else 0.
"""
import argparse
import collections
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DEFAULT_LAYER = os.path.join(REPO, "src", "pes", "translated")
RENDEZVOUS = os.path.join(REPO, "tools", "dev", "rendezvous")

CALL_RE = re.compile(r"\bpes_fn_([0-9A-Fa-f]+)\(s\)")
GAP_RE = re.compile(r"/\*OUT-OF-SET CALL (0x[0-9A-Fa-f]+)\*/")
FILE_RE = re.compile(r"pes_fn_([0-9A-Fa-f]+)\.cpp$")

# rendezvous/rv.c samples calls 1..FIRST, then every EVERY-th. A new hook starts at EVERY 20 (brief rule). FIRST 1 keeps
# the first call, the one a proof replays first.
HOOK_FIRST_CALLS = 1
HOOK_EVERY_CALLS = 20
# hook_plan.py reads this many bytes at each entry; the window check covers 14 of them (rv_hooks.displaced_window).
HOOK_READ_BYTES = 64

EXIT_REFUSED = 1
EXIT_ROOT_NOT_TRANSLATED = 2


def parse_va(text):
    return int(text, 16)


def layer_index(layer):
    """{VA: path} for every pes_fn_<VA>.cpp in the layer's group directories."""
    index = {}
    for path in glob.glob(os.path.join(layer, "*", "pes_fn_*.cpp")):
        index[int(FILE_RE.search(path).group(1), 16)] = path
    return index


def scan(path):
    """(translated callee VAs, [(untranslated target VA, 1-based line)]) of one function's file."""
    with open(path) as f:
        text = f.read()
    callees = {int(m.group(1), 16) for m in CALL_RE.finditer(text)}
    gaps = [(int(m.group(1), 16), text.count("\n", 0, m.start()) + 1) for m in GAP_RE.finditer(text)]
    return callees, gaps


def reach(index, roots):
    """({target VA: [(caller VA, line), ...]}, translated functions walked) for every call reachable from roots."""
    gaps = collections.defaultdict(list)
    walked = set()
    todo = collections.deque(roots)
    while todo:
        va = todo.popleft()
        if va in walked:
            continue
        if va not in index:
            raise SystemExit("layer calls pes_fn_%X, which it does not hold" % va)
        walked.add(va)
        callees, here = scan(index[va])
        todo.extend(callees)
        for target, line in here:
            gaps[target].append((va, line))
    return gaps, walked


def plan_hooks(targets, code_at, entry_of, first, every, prefix, displaced_window):
    """hook_plan rows and hooks.cfg lines for {VA: note}. entry_of(va) is the .pdata entry holding va, or None.
    Returns (rows, cfg lines, refusals [(VA, reason)])."""
    rows, cfg, refused = [], [], []
    for va in sorted(targets):
        name = "%s_%X" % (prefix, va)
        rows.append('    ("%s", 0x%X, "%s"),' % (name, va, targets[va]))
        entry = entry_of(va)
        if entry is None:
            refused.append((va, "not inside any .pdata function"))
            continue
        if entry != va:
            refused.append((va, "not a function entry (inside 0x%X)" % entry))
            continue
        code = code_at(va)
        length, why = displaced_window(code, va)
        if why:
            refused.append((va, why))
            continue
        cfg.append("hook %s %#x %d %d %d %s" % (name, va, length, first, every, code[:length].hex()))
    return rows, cfg, refused


def hooks_for_image(image, targets, first, every, prefix):
    if RENDEZVOUS not in sys.path:
        sys.path.insert(0, RENDEZVOUS)
    import hook_plan as hp  # imports rv_hooks (capstone): only --hooks needs it

    img = hp.rh.Image(image, flat=True)
    funcs = hp.load_pdata(img)
    starts = [b for b, _ in funcs]

    def entry_of(va):
        fn = hp.function_of(funcs, starts, va - img.base)
        return None if fn is None else img.base + fn[0]

    return plan_hooks(targets, lambda va: img.read(va, HOOK_READ_BYTES), entry_of, first, every, prefix,
                      hp.rh.displaced_window)


def main(argv=None):
    ap = argparse.ArgumentParser(description="list the untranslated callees reachable from translated functions")
    ap.add_argument("--layer", default=DEFAULT_LAYER, help="translated layer (default src/pes/translated)")
    ap.add_argument("--hooks", action="store_true", help="also print hook_plan TARGETS rows and hooks.cfg lines")
    ap.add_argument("--image", default=os.environ.get("PES_EXE"), help="flat unpacked image for --hooks")
    ap.add_argument("--first", type=int, default=HOOK_FIRST_CALLS, help="hooks.cfg FIRST (calls 1..FIRST sampled)")
    ap.add_argument("--every", type=int, default=HOOK_EVERY_CALLS, help="hooks.cfg EVERY (then every EVERY-th call)")
    ap.add_argument("--prefix", default="callee", help="hook name prefix: NAME is PREFIX_<VA>")
    ap.add_argument("roots", nargs="+", type=parse_va, metavar="VA")
    args = ap.parse_args(argv)

    index = layer_index(args.layer)
    absent = [r for r in args.roots if r not in index]
    if absent:
        print("not translated in %s: %s" % (args.layer, " ".join("0x%X" % r for r in absent)), file=sys.stderr)
        return EXIT_ROOT_NOT_TRANSLATED
    gaps, walked = reach(index, args.roots)
    print("roots %s: %d translated functions walked, %d untranslated callee(s)"
          % (" ".join("0x%X" % r for r in args.roots), len(walked), len(gaps)))
    for target in sorted(gaps):
        sites = " ".join("0x%X:%d" % site for site in sorted(gaps[target]))
        stale = "  (IN LAYER: stale marker, regenerate)" if target in index else ""
        print("0x%X  call sites: %s%s" % (target, sites, stale))
    if not args.hooks:
        return 0
    if not args.image:
        ap.error("--hooks needs --image or PES_EXE")
    targets = {t: "untranslated callee; call sites %s" % " ".join("0x%X:%d" % s for s in sorted(gaps[t]))
               for t in gaps}
    rows, cfg, refused = hooks_for_image(args.image, targets, args.first, args.every, args.prefix)
    print("# hook_plan.py TARGETS rows")
    print("\n".join(rows))
    print("# hooks.cfg lines (hook NAME VA LEN FIRST EVERY BYTES)")
    print("\n".join(cfg))
    for va, why in refused:
        print("# REFUSE 0x%X: %s" % (va, why))
    return EXIT_REFUSED if refused else 0


if __name__ == "__main__":
    sys.exit(main())
