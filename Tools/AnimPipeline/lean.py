import json,sys,zipfile,numpy as np
sys.path.insert(0,'.')
from animpipe import author as A
def card_from_zip(z):
    zf=zipfile.ZipFile(z); n=[x for x in zf.namelist() if x.endswith('overlord_dair_v4.json')][0]; return json.loads(zf.read(n))
def lean(c,frames):
    fr=A.frames(c['author']); po=A.Poser(); out={}
    for f in frames:
        P,R=po.solve(fr[f]); pv=np.array(P['pelvis']); h=np.array(P['spine_05'])-pv
        out[f]=(round(float(np.degrees(np.arctan2(h[0],h[2]))),1), round(float(np.degrees(np.arctan2(h[1],h[2]))),1))
    return out
if __name__=='__main__':
    pre=card_from_zip('../../../GASP_Giustino2/Saved/AnimPipeline/snapshots/overlord_dair_v4/20261103_182459.zip')
    cur=json.load(open('cards/overlord/overlord_dair_v4.json'))
    fs=[0,3,14,15,31,37,42]
    print('pre-A (x-lean,y-lean deg)',lean(pre,fs)); print('now  ',lean(cur,fs))
