#!/usr/bin/env python3
"""Turns live_timeline_poll.py logs into the episode file the C++ oracle (tests/pes_cutscene_timeline_test.cpp) reads.
usage: timeline_episodes.py LOG.jsonl [LOG.jsonl ...] > episodes.txt     (then GF_CUT_EPISODES=episodes.txt)

An episode is one DemoInfo that lives (D.scene != 0) while the manager plays steps. Tick 0 = the frame of the first row
that shows the DemoInfo. Per step: length = the player's length (+0x64) at the step's first update; start = the clock at
that row when it is >= 5 ticks (a start jump; the clock of a 0-start step has advanced at most a few 1.111-tick updates by
then), else 0; speed 1 (the Cut record's playSpeed was 1.0 on every logged step: the clock moves 1.1111 ticks per update).
Rows are kept from the first DemoInfo row to its end. Manager and player columns are '-' until the go flag (M.f54) is set:
before it the manager is still loading and the timeline does not model that. Row line: row F F1 DEMO PHASE STEP PSTATE PT TOT
(F1 = the frame counter re-read after the objects, -1 in logs from before the poller did that).
PES-derived: the output is a capture; keep it in scratch, never commit it."""
import json
import sys


def episodes(path):
    rows = [json.loads(l) for l in open(path)]
    cur = None
    for r in rows:
        if 'f' not in r:
            continue
        d = r.get('D')
        if d is None:
            continue
        if cur is None:
            if d['st'] == 0:
                continue
            cur = {'p': r['f'], 'rows': [], 'steps': [], 'kind': d['kind']}
        m = r.get('M', {})
        go = m.get('f54') == 1
        if go and m.get('ps') == 5 and (not cur['steps'] or cur['steps'][-1]['step'] != m['step']):
            pt = m.get('pt', 0.0)
            cur['steps'].append({'step': m['step'], 'len': m['plen'], 'start': pt if pt >= 5 else 0.0})
        cur['rows'].append((r['f'], r.get('f1', -1), d['st'], m if go else None))
        if d['st'] == 0:
            yield cur
            cur = None
        continue
    if cur and cur['rows']:
        yield cur


def main():
    n = 0
    for path in sys.argv[1:]:
        for e in episodes(path):
            if e['kind'] != 1 or not e['steps']:
                continue
            n += 1
            print('episode %s_%d %d' % (path.split('/')[-1], n, e['p']))
            for s in e['steps']:
                print('step %.3f %.3f 1.0' % (s['len'], s['start']))
            for f, f1, st, m in e['rows']:
                if m is None:
                    print('row %d %d %d - - - - -' % (f, f1, st))
                else:
                    pt = m.get('pt')
                    print('row %d %d %d %d %d %s %s %.4f' % (f, f1, st, m['p1d8'], m['step'],
                          m['ps'] if 'ps' in m else '-', '%.3f' % pt if pt is not None else '-', m['tot']))
            print('end')


if __name__ == '__main__':
    main()
