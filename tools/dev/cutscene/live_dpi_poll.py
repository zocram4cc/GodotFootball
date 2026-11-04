#!/usr/bin/env python3
"""Read-only live poller of the goal celebration run: while a DemoInfo goal scene (A0+8 != 0) is up, logs per frame the changed
DemoPlayerInfo (G+(i+0xD9)*0x28, 0x80 bytes), man (+0x5A0..+0x600 of G+(i+0xBF)*0x28) and slot-record (+0x1400..+0x1440) bytes of the 22 men.
usage: live_dpi_poll.py PID OUT.jsonl [seconds]"""
import sys,os,subprocess,struct,time,json
pid=int(sys.argv[1]); out=open(sys.argv[2],'a'); secs=float(sys.argv[3]) if len(sys.argv)>3 else 3600
srv=subprocess.Popen(['sudo','-n','python3',os.path.join(os.path.dirname(os.path.abspath(__file__)),'live_srv.py')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1)
def rd(*rs):
    srv.stdin.write('%d %s\n'%(pid,','.join('%x:%d'%r for r in rs))); srv.stdin.flush()
    l=srv.stdout.readline().rstrip('\n')
    if l=='E': raise SystemExit('no pid')
    return [None if x=='x' else bytes.fromhex(x) for x in l.split(' ')]
U64=lambda b:struct.unpack('<Q',b)[0]
GVA=0x1436F3DC8; ROOT=0x1436F3F88
last={}; t0=time.time(); lastf=None
while time.time()-t0<secs:
    g,root=rd((GVA,8),(ROOT,8))
    G=U64(g) if g else 0; D=U64(root) if root else 0
    if not G or not D: time.sleep(0.05); continue
    d=rd((D,0x320),)[0]
    if not d: time.sleep(0.05); continue
    sc=struct.unpack('<I',d[0x1f0:0x1f4])[0]
    if sc==0: last={}; time.sleep(0.05); continue
    mi=U64(rd((G+0x270,8))[0])
    f=struct.unpack('<i',rd((mi+0x1198,4))[0])[0] if mi else -1
    if f==lastf: time.sleep(0.003); continue
    lastf=f
    dp=rd(*[(G+(i+0xd9)*0x28,8) for i in range(22)]); mp=rd(*[(G+(i+0xbf)*0x28,8) for i in range(22)]); sp=rd(*[(G+(i+0xf3)*0x28,8) for i in range(22)])
    dd=rd(*[(U64(x),0x80) for x in dp if x]); md=rd(*[(U64(x)+0x5a0,0x60) for x in mp if x]); sd=rd(*[(U64(x)+0x1400,0x40) for x in sp if x])
    row={'f':f,'t':round(time.time()-t0,3),'kind':struct.unpack('<I',d[0x8:0xc])[0],'st':struct.unpack('<I',d[0x1d8:0x1dc])[0],'sc':sc,'ready':d[0x1e8+0xec],'T':[round(struct.unpack('<f',d[0x1e8+0x30+4*k:0x1e8+0x34+4*k])[0],2) for k in (0,2)]}
    ch={}
    for i in range(22):
        cur=(dd[i].hex() if dd[i] else None, md[i].hex() if md[i] else None, sd[i].hex() if sd[i] else None)
        if last.get(i)!=cur: ch[i]=cur; last[i]=cur
    if ch: row['c']=ch
    out.write(json.dumps(row)+'\n'); out.flush()
    if out.tell()>60_000_000: break
