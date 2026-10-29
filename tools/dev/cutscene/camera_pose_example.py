"""Worked example: PES FixDemoCamera CANM pose, hand-computed from the decode (float32 throughout).
usage: PES21_ROOT=DIR [PESFMT_DUMP=build/apps/pesfmt_dump] python camera_pose_example.py FDC [CLOCK_TICKS ...]
(CANM bytes: embedded resource, else `pesfmt_dump cat`). Independent float32 oracle for tests/pes_fdc_test.cpp
PesFdcPoseInstall.EntranceCutPoseMatchesTheWorkedExampleBitForBit (the test pins this script's rows).
Every step cites the VA it mirrors; see docs/re/pes-re-camera-pose.md."""
import os, sys, struct, subprocess, math
import numpy as np
F = np.float32
DUMP = os.environ.get('PESFMT_DUMP', 'build/apps/pesfmt_dump')
ROOT = os.environ['PES21_ROOT']
u16 = lambda b, o: struct.unpack_from('<H', b, o)[0]
s16 = lambda b, o: struct.unpack_from('<h', b, o)[0]
u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
f32 = lambda b, o: F(struct.unpack_from('<f', b, o)[0])
cstr = lambda b, o: b[o:b.index(b'\0', o)].decode()

def container(b):
    n = u32(b, 0)
    return [(u32(b, 4 + 12 * i), u32(b, 8 + 12 * i), cstr(b, u32(b, 12 + 12 * i))) for i in range(n)]

def cameras(b):
    off, size, _ = container(b)[0]
    blk = b[off:off + size]
    m = u32(blk, 0); out = []
    for i in range(m):
        ro, rs, to = struct.unpack_from('<III', blk, 4 + 12 * i)
        if blk[to] != 6: continue
        r = blk[ro:ro + rs]
        out.append(dict(idx=i, key=u32(r, 0), start=f32(r, 4), path=cstr(r, 0x0c), animStart=f32(r, 0x8c),
                        near=f32(r, 0x90), far=f32(r, 0x94), b9c=r[0x9c], loop=r[0x9d], b9e=r[0x9e],
                        heightRate=f32(r, 0xa0), offsetFlag=r[0xa6], fn=[u16(r, 0xa8 + 2 * k) for k in range(4)],
                        target=struct.unpack_from('<b', r, 0xb1)[0]))
    return out

def canm_bytes(fdc, path):
    for off, size, name in container(fdc)[1:]:
        if name.endswith(path):
            if size != 4: return fdc[off:off + size]
            break
    return subprocess.run([DUMP, '--pes21-root', ROOT, 'cat', path], capture_output=True, check=True).stdout

class Canm:  # 0x142139d60 (load): +0xa0 track, +0xa8 = (float)s16(+4), +0xac = fps
    def __init__(s, b):
        s.b = b
        s.track = u32(b, 0x10)
        s.length = F(s16(b, 4))
        s.fps = F(u32(b, 8)) if b[0xc] & 1 else F(30.0)  # 0x142139e30 movss [0x142591140]=30.0

    def channel(s, c):  # 0x142139bb7: u32 at track + 8 + 4c, relative to track
        return s.track + u32(s.b, s.track + 8 + 4 * c)

def key_index(b, ch, iframe):  # 0x14213a210
    cnt = s16(b, ch + 4)
    if u32(b, ch + 8) == 0:
        return min(iframe, cnt - 1)
    t = ch + u32(b, ch + 8)
    i2 = cnt - 1
    if iframe < u16(b, t + 2 * i2):
        i4, i1 = i2, 0
        while True:
            i2 = i1
            if not 1 < i4: break
            i5 = i4 >> 1; i3 = i4 - i5; i4 = i5
            if u16(b, t + 2 * (i5 + i2)) <= iframe: i4 = i3; i1 = i5 + i2
    return i2

def ktime(b, ch, i):
    if u32(b, ch + 8) == 0: return 0 if s16(b, ch + 4) == 1 else i
    return u16(b, ch + u32(b, ch + 8) + 2 * i)

def sample(b, ch, frame, iframe):  # 0x142139eb0 (ktAnimationBase vfunc +0x18)
    typ, cnt, sem = s16(b, ch), s16(b, ch + 4), b[ch + 6]
    vals = ch + u32(b, ch + 0xc)
    i0 = key_index(b, ch, iframe); t0 = ktime(b, ch, i0); i = i0
    while True:
        a = i
        if cnt - 1 <= i: f = F(0.0); i1 = a; break
        i = a + 1; t1 = ktime(b, ch, i)
        if t1 != t0:
            f = F(F(frame - F(t0)) / F(t1 - t0)); i1 = i; break
    i0 = a
    if typ in (1, 3):    # table 0x143542630/50 -> 0x14213a180 quaternion
        q0 = np.frombuffer(b, F, 4, vals + 16 * i0); q1 = np.frombuffer(b, F, 4, vals + 16 * i1)
        if f < F(0.001): v = q0.copy()          # 0x14213a184 [0x1425a9668]=0.001
        elif f > F(0.999): v = q1.copy()        # 0x14213a1ac [0x1425e4d78]=0.999
        else: v = slerp(q0, q1, f)
        return sem, v
    if typ in (4, 5, 6, 10):  # 0x14213a110 vec3: f*v1 + (1-f)*v0
        v0 = np.frombuffer(b, F, 3, vals + 16 * i0); v1 = np.frombuffer(b, F, 3, vals + 16 * i1)
        return sem, f * v1 + (F(1) - f) * v0
    if typ == 8:          # 0x14213a0e0 scalar: (1-f)*v0 + f*v1
        return sem, (F(1) - f) * f32(b, vals + 4 * i0) + f * f32(b, vals + 4 * i1)
    return sem, None

def slerp(q0, q1, t):  # 0x140a93210
    d = F(q1[0] * q0[0] + q1[1] * q0[1] + q0[2] * q1[2] + q0[3] * q1[3])
    sgn = F(1)
    if d < 0: d = -d; sgn = F(-1)
    if F(1) - d <= F(9.999999747378752e-06):   # 0x140a932aa [0x1425568d0]
        w0, w1 = F(1) - t, t
    else:
        th = F(math.acos(d)); inv = F(1) / F(math.sin(th))
        w0 = F(math.sin(F(F(1) - t) * th)) * inv; w1 = F(math.sin(th * t)) * inv
    w1 = w1 * sgn
    return w0 * q0 + w1 * q1

def quat_to_mat(q):  # 0x140a91ca0, s = 2/|q|^2, column-vector rotation
    x, y, z, w = q; n = F(x * x + y * y + z * z + w * w); s = F(2) / n if n > 0 else F(0)
    return np.array([[1 - (z * z * s + y * y * s), x * y * s - w * z * s, w * y * s + z * x * s],
                     [w * z * s + x * y * s, 1 - (z * z * s + x * x * s), z * y * s - w * x * s],
                     [z * x * s - w * y * s, w * x * s + z * y * s, 1 - (y * y * s + x * x * s)]], F)

def evaluate(c, frame):  # 0x142139ab0, non-loop (+0xb0 == 0): clamp to [0, length]
    frame = c.length if frame > c.length else max(F(0), frame)
    iframe = int(math.floor(frame))   # floorf thunk 0x1415a1876 then cvttss2si
    out = {}
    for k in range(6):
        sem, v = sample(c.b, c.channel(k), frame, iframe)
        if sem == 0:
            m = quat_to_mat(v); m[:, 0] *= -1; m[:, 2] *= -1   # 0x142139cab..ce8: negate columns 0 and 2
            out['rot'] = m
        elif sem == 1: out['pos'] = v
        elif sem == 2: out['fov'] = v * F(3.1415927410125732) / F(180.0)  # 0x142139c19/c22
        elif sem in (3, 4): out['clip%d' % sem] = v
    return frame, out

def pose(rec, c, clock):
    t = clock if rec['b9c'] == 0 else max(F(0), clock - rec['start'])  # 0x141f42191..c9
    t = F(t + rec['animStart'])                                       # 0x141f7b4fc +0x21c
    frame = F(t * F(c.fps / F(60.0)))                                 # 0x141f7b51a [0x14259a764]=60.0
    if rec['loop'] and frame > c.length and int(frame / c.length) != 0:
        frame = F(frame - F(int(frame / c.length)) * c.length)
    frame, e = evaluate(c, frame)
    m, p = e['rot'], e['pos']
    return dict(clock=clock, frame=float(frame), eye=p, forward=m[:, 2], up=m[:, 1],
                fovV_deg=float(e['fov']) * 180 / math.pi, near=rec['near'], far=rec['far'],
                canm_near=e.get('clip3'), canm_far=e.get('clip4'))

if __name__ == '__main__':
    fdc = open(sys.argv[1], 'rb').read()
    cams = cameras(fdc)
    for r in cams: print({k: (round(float(v), 4) if isinstance(v, np.floating) else v) for k, v in r.items()})
    offs = [float(x) for x in sys.argv[2:]] or [30.0, 91.0, 180.0]  # ticks after startCutTime: 0.5 s, mid-key, 3 s
    for r in cams[:2]:
        c = Canm(canm_bytes(fdc, r['path']))
        print(f"\n{r['path']}: length {c.length} frames @ {c.fps} fps; channels (type, keys, semantic, timesOff):",
              [(s16(c.b, c.channel(k)), s16(c.b, c.channel(k) + 4), c.b[c.channel(k) + 6], u32(c.b, c.channel(k) + 8)) for k in range(6)])
        for clock in [r['start'] + F(x) for x in offs]:
            p = pose(r, c, F(clock))
            print(f"  clock {float(clock):7.1f} ticks -> frame {p['frame']:8.3f}: eye {np.round(p['eye'], 4)} fwd {np.round(p['forward'], 4)} up {np.round(p['up'], 4)} fovV {p['fovV_deg']:.4f} deg near {float(p['near'])} far {float(p['far'])} (canm near/far {p['canm_near']}/{p['canm_far']})")
