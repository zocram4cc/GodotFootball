#!/usr/bin/env python3
"""expire.py: the capture expiry DRY RUN. Lists the captures that no results record and no open lane cites, and that are
older than --days, with their bytes. Deletes nothing: deleting needs the owner's approval.

Usage: python3 tools/dev/rendezvous/expire.py --live2 DIR --record GLOB [--record GLOB ...] [--cited FILE ...] [--days 14]
A capture is cited when a record names it as live2/pidN/seqM, or names its launch as live2/pidN (a record that names a
launch without its seq numbers cites every capture of that launch). --cited files list the same forms, one per line,
'#' comments allowed. Archived captures are sized through their links (the bytes are the archive's).
"""
import argparse
import glob
import os
import re
import sys
import time

DAY = 86400
CITE = re.compile(r"pid(\d+)(?:/seq(\d+))?")


def citations(text):
    """(launch, seq) pairs named in text; (launch, None) when only the launch is named."""
    return {("pid" + launch, ("seq" + seq) if seq else None) for launch, seq in CITE.findall(text)}


def read_cited(paths):
    cited = set()
    for p in paths:
        with open(p) as f:
            cited |= citations(f.read())
    return cited


def allocated(d):
    return sum(os.stat(os.path.join(d, f)).st_blocks * 512 for f in os.listdir(d))


def plan(live2, record_texts, cited, days, now):
    """[(token, capture dir, bytes)] of the captures older than days that no record and no open lane cites."""
    named = set(cited)
    for text in record_texts:
        named |= citations(text)
    out = []
    for d in sorted(glob.glob(os.path.join(live2, "pid*", "seq*"))):
        launch, seq = os.path.basename(os.path.dirname(d)), os.path.basename(d)
        if (launch, seq) in named or (launch, None) in named:
            continue
        if now - os.path.getmtime(d) < days * DAY:
            continue
        out.append(("%s/%s" % (launch, seq), d, allocated(d)))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--live2", required=True)
    ap.add_argument("--record", action="append", default=[], help="a results record (glob, repeatable)")
    ap.add_argument("--cited", action="append", default=[], help="an open lane's citation file (repeatable)")
    ap.add_argument("--days", type=float, default=14.0)
    a = ap.parse_args(argv)
    if not a.record:
        ap.error("give --record: the records to scan (a scan without them would list every capture)")
    texts = []
    for pat in a.record:
        for p in sorted(glob.glob(pat)):
            with open(p) as f:
                texts.append(f.read())
    got = plan(a.live2, texts, read_cited(a.cited), a.days, time.time())
    for token, _, nbytes in got:
        print("EXPIRE-CANDIDATE %s %.2f GB" % (token, nbytes / 1e9))
    total = sum(nbytes for _, _, nbytes in got)
    print("dry run: %d captures, %.2f GB eligible; nothing deleted" % (len(got), total / 1e9))
    return 0


if __name__ == "__main__":
    sys.exit(main())
