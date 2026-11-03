import json,copy,sys,math,numpy as np
sys.path.insert(0,'.'); sys.path.insert(0,'.')
from animpipe import author as A, card as C
import lean as Ln
S37,S42=float(sys.argv[1]),float(sys.argv[2])
p=sys.argv[3]
c=json.load(open(p));au=c['author']['poses'];fr=A.frames(c['author'])
B31=au['31']['bones']; w="owner 03/11 D: busto FAF avanti un pelo (lean +~6 deg a f42), salita lenta"
def mix(a,b,s): return [x+(y-x)*s for x,y in zip(a,b)]
for f,s in (('37',S37),('42',S42)):
    q=au[f]
    q['spine']=mix(au['31']['spine'],q['spine'],s); q['pelvis']['rot']=mix(au['31']['pelvis']['rot'],q['pelvis']['rot'],s)
    q['pelvis']['loc']=mix(au['31']['pelvis']['loc'],q['pelvis']['loc'],s)
    for k,v in q['spine_chain'].items(): q['spine_chain'][k]=mix(au['31']['spine_chain'][k],v,s)
    for b in ['pelvis','spine_01','spine_02','spine_03','spine_04','spine_05']:
        a,z=np.array(B31[b]),np.array(q['bones'][b])
        if a@z<0: z=-z
        m=a+(z-a)*s; q['bones'][b]=list(map(float,m/np.linalg.norm(m)))
    for pe in c['poses']:
        if pe['frame']==int(f): pe['source']['keep']={k:w for k in ('trunk','head','arm_l','arm_r','leg_l','leg_r')}
# follow-through: driver = pelvis rebound after the stomp, damped sine (period 8 f, tau 6 f); each chain link lags more
def osc(t,per=8.0,tau=6.0): return 0.0 if t<0 else math.exp(-t/tau)*math.sin(2*math.pi*t/per)
def dip(t,per=8.0,tau=6.0): return 0.0 if t<0 else math.exp(-t/tau)*0.5*(1-math.cos(2*math.pi*t/per))
def win(f): x=min(max((f-24)/6.0,0),1); return 1-x*x*(3-2*x)
T0=17
for f in range(18,31):
    q=copy.deepcopy(fr[f]); q.pop('bones',None); k=win(f)
    q['pelvis']['loc'][2]-=  4.0*dip(f-T0)*k          # driver, no lag
    q['spine'][0]        +=  5.0*osc(f-T0-1)*k         # spine +1
    q['head'][0]         += 12.0*osc(f-T0-2)*k         # head +2
    for a in ('arm_l','arm_r'): q[a]['hand'][2]-=  3.0*dip(f-T0-3)*k   # hands +3
    for a in ('leg_l','leg_r'): q[a]['pole'][2]+= 4.0*osc(f-T0-2)*k    # knees +2 (feet stay planted)
    au[str(f)]=q
    c['poses'].append({"name":f"follow_{f}","frame":f,"source":None,"origin":"claude","why":"owner 03/11 D: follow-through dopo lo stomp, catena in ritardo (bacino, spina+1, testa+2, mani+3, ginocchia+2)"})
c['poses'].sort(key=lambda q:q['frame']); c['author']['poses']={k:au[k] for k in sorted(au,key=int)}
json.dump(c,open(p,'w'),indent=1); C.load(p)
print('lean y',Ln.lean(c,[0,31,37,42]))
