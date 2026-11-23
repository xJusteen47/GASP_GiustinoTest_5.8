import cv2, numpy as np, sys
img=cv2.imread(sys.argv[1]); H,W=img.shape[:2]; c=np.array([W/2,H/2])
g=cv2.cvtColor(img,cv2.COLOR_BGR2GRAY); e=cv2.Canny(g,40,120)
m=np.zeros_like(g); m[150:700, :]=255; m[520:720,640:840]=0
e=cv2.bitwise_and(e,m)
L=np.asarray(cv2.HoughLinesP(e,1,np.pi/360,50,minLineLength=60,maxLineGap=3)).reshape(-1,4).astype(float)
ang=np.degrees(np.arctan2(L[:,3]-L[:,1],L[:,2]-L[:,0]))%180
S=L[(ang>78)&(ang<102)]
A=[]
for x1,y1,x2,y2 in S:
    l=np.cross([x1,y1,1],[x2,y2,1]); A.append(l/np.linalg.norm(l[:2]))
A=np.array(A); rng=np.random.default_rng(1); best=None
for _ in range(3000):
    i,j=rng.choice(len(A),2,replace=False); p=np.cross(A[i],A[j])
    if abs(p[2])<1e-9: continue
    p=p/p[2]; inl=np.abs(A@p)<2.0
    if p[1]>H and (best is None or inl.sum()>best[0]): best=(inl.sum(),inl,p)
n,inl,_=best; _,_,V=np.linalg.svd(A[inl]); p=V[-1]; p=p/p[2]
yh=float(sys.argv[2]); yv=p[1]
f=np.sqrt((c[1]-yh)*(yv-c[1]))
print(f"vertical lines {n}/{len(A)} VP=({p[0]:.0f},{yv:.0f}); f={f:.0f}px HFOV={2*np.degrees(np.arctan(c[0]/f)):.1f} VFOV={2*np.degrees(np.arctan(c[1]/f)):.1f} pitch={np.degrees(np.arctan((c[1]-yh)/f)):.1f}")
# sensitivity: horizon +-30 px
for dy in (-30,30):
    f2=np.sqrt((c[1]-yh-dy)*(yv-c[1])); print(f"  horizon {yh+dy:.0f}: HFOV={2*np.degrees(np.arctan(c[0]/f2)):.1f} pitch={np.degrees(np.arctan((c[1]-yh-dy)/f2)):.1f}")
dbg=img.copy()
for (x1,y1,x2,y2) in S[inl].astype(int): cv2.line(dbg,(x1,y1),(x2,y2),(0,0,255),2)
cv2.imwrite('dbg_v.png',dbg)
