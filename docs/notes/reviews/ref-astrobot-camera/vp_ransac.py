import cv2, numpy as np, sys
img=cv2.imread(sys.argv[1]); H,W=img.shape[:2]; c=np.array([W/2,H/2])
g=cv2.cvtColor(img,cv2.COLOR_BGR2GRAY); e=cv2.Canny(g,40,120)
L=np.asarray(cv2.HoughLinesP(e,1,np.pi/720,50,minLineLength=70,maxLineGap=4)).reshape(-1,4).astype(float)
ang=np.degrees(np.arctan2(L[:,3]-L[:,1],L[:,2]-L[:,0]))%180
def homog(S):
    A=[]
    for x1,y1,x2,y2 in S:
        l=np.cross([x1,y1,1],[x2,y2,1]); A.append(l/np.linalg.norm(l[:2]))
    return np.array(A)
def ransac(S,cond,thr=2.0,it=4000,seed=0):
    A=homog(S); rng=np.random.default_rng(seed); best=None
    for _ in range(it):
        i,j=rng.choice(len(A),2,replace=False); p=np.cross(A[i],A[j])
        if abs(p[2])<1e-9: continue
        p=p/p[2]
        if not cond(p): continue
        inl=np.abs(A@p)<thr
        if best is None or inl.sum()>best[0]: best=(inl.sum(),inl)
    n,inl=best; _,_,V=np.linalg.svd(A[inl]); p=V[-1]; p=p/p[2]
    # bootstrap spread
    ps=[]
    idx=np.where(inl)[0]
    for k in range(200):
        s=rng.choice(idx,len(idx)); _,_,V=np.linalg.svd(A[s]); q=V[-1]; ps.append(q[:2]/q[2])
    ps=np.array(ps)
    return p[:2], n, len(A), ps.std(0), S[inl]
vert=(ang>80)&(ang<100); depth=((ang>15)&(ang<80))|((ang>100)&(ang<165))
pv,nv,tv,sv,Sv=ransac(L[vert],lambda p:p[1]>H or p[1]<0, thr=1.5)
pd,nd,td,sd,Sd=ransac(L[depth],lambda p:p[1]<c[1] and 0<p[0]<W, thr=2.0)
print(f"W,H={W},{H}")
print(f"vertical VP {pv.round(0)} inliers {nv}/{tv} sd {sv.round(0)}")
print(f"depth VP {pd.round(0)} inliers {nd}/{td} sd {sd.round(0)}")
dbg=img.copy()
for x1,y1,x2,y2 in Sv.astype(int): cv2.line(dbg,(x1,y1),(x2,y2),(0,0,255),2)
for x1,y1,x2,y2 in Sd.astype(int): cv2.line(dbg,(x1,y1),(x2,y2),(0,255,0),2)
cv2.circle(dbg,(int(pd[0]),int(pd[1])),8,(255,0,255),-1)
cv2.imwrite(sys.argv[2],cv2.resize(dbg,(W*2//3,H*2//3)))
np.save('vps.npy',np.array([pv,pd]))
