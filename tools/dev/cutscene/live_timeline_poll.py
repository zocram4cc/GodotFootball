#!/usr/bin/env python3
"""Read-only live poller of PES's cutscene timeline: logs one JSONL row whenever a tracked field changes.
usage: poll.py PID OUT.jsonl [seconds]"""
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
last=None; lastf=None; t0=time.time(); n=0
while time.time()-t0<secs:
    g,root=rd((GVA,8),(ROOT,8))
    G=U64(g) if g else 0; D=U64(root) if root else 0
    if not G: time.sleep(0.05); continue
    mi=U64(rd((G+0x270,8))[0])
    row={}
    if mi:
        b=rd((mi+0x1198,4),(mi+0x1168,8),(mi+0x1554,8),(mi+0x1574,4))
        row['f']=struct.unpack('<i',b[0])[0]; per,play=struct.unpack('<ii',b[1]); row['per']=per; row['play']=play
        row['rs']=struct.unpack('<ii',b[2]); row['why']=struct.unpack('<i',b[3])[0]
    if D:
        d=rd((D,0x910),)[0]
        if d:
            row['D']=dict(scene=struct.unpack('<I',d[0:4])[0],kind=struct.unpack('<I',d[8:12])[0],st=struct.unpack('<I',d[0x1d8:0x1dc])[0],w74=struct.unpack('<I',d[0x1d0:0x1d4])[0],b75=d[0x1d4],b77=d[0x1dc],s8f2=d[0x8f2],c8f3=d[0x8f3],ctr=struct.unpack('<I',d[0x900:0x904])[0],v868=struct.unpack('<i',d[0x868:0x86c])[0],b870=d[0x870],sel=d[0x8f0],k878=struct.unpack('<4I',d[0x878:0x888]))
            M=U64(d[0x1e0:0x1e8])
            if M:
                m=rd((M,0x270),)[0]
                if m:
                    f=lambda o:struct.unpack('<f',m[o:o+4])[0]; u=lambda o:struct.unpack('<I',m[o:o+4])[0]
                    mm=dict(vt=U64(m[0:8]),p1d4=u(0x1d4),p1d8=u(0x1d8),f54=m[0x54],f55=m[0x55],f4e=m[0x4e],step=m[0xe8],tot=round(f(0x100),4),w21c=u(0x21c),nst=(U64(m[0x28:0x30])-U64(m[0x20:0x28]))//0x1e0,w254=round(f(0x254),1))
                    P=U64(m[0xe0:0xe8])
                    if P:
                        p=rd((P,0xb00),)[0]
                        if p: mm.update(ps=struct.unpack('<I',p[0x58:0x5c])[0],pt=round(struct.unpack('<f',p[0x60:0x64])[0],3),plen=round(struct.unpack('<f',p[0x64:0x68])[0],3),pstate5c=struct.unpack('<I',p[0x5c:0x60])[0],pstart=round(struct.unpack('<f',p[0x70:0x74])[0],3),pade=p[0xade],pad8=p[0xad8],pada=p[0xada])
                    row['M']=mm
    if mi and 'M' in row: row['f1']=struct.unpack('<i',rd((mi+0x1198,4),)[0])[0]  # frame re-read after the objects: f != f1 marks a row that straddles a frame
    key=json.dumps({k:v for k,v in row.items() if k not in('f','f1')},sort_keys=True)
    fkey=row.get('f') if ('D' in row or 'M' in row) else None
    if key!=last or (fkey is not None and fkey!=lastf):
        row['t']=round(time.time()-t0,3); out.write(json.dumps(row)+'\n'); out.flush(); last=key; lastf=fkey; n+=1
    time.sleep(0.004)
