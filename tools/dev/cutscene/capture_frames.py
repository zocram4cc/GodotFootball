"""capture_frames.py: MI frame counter (MI+0x1198), period, play, restart kind and stop reason of every goal/foul/cutscene
rendezvous capture. usage (PES_EXE set): capture_frames.py LIVE2_ROOT"""
import sys,os,glob,json
from capture_sm import info
R=sys.argv[1]
keys=('goal_pick','scene_start','cut_offset','cut_anchor','foul_sanction','judge_whistle','fixdemo')
rows=[]
for p in sorted(glob.glob(R+'/pid*')):
    for s in sorted(glob.glob(p+'/seq*'),key=lambda x:int(x.split('seq')[-1])):
        try:
            j=json.load(open(s+'/index.json')); h=j['rv']['hook']
        except Exception: continue
        if not any(k in h for k in keys): continue
        try:
            o,_=info(s)
        except Exception as e:
            o={'err':str(e)[:40]}
        print(p.split('/')[-1],s.split('/')[-1],h[:26],j['rv']['calls'],o.get('frame'),o.get('per'),o.get('play'),o.get('restart'),o.get('reason'),flush=True)
