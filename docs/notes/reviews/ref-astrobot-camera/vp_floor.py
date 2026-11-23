import cv2, numpy as np, sys
img = cv2.imread(sys.argv[1]); H, W = img.shape[:2]
g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
mask = np.zeros_like(g)
# floor regions (hand-picked polygons): front platform, right big floor
cv2.fillPoly(mask, [np.array([[400,545],[1050,545],[1150,710],[300,710]])], 255)
cv2.fillPoly(mask, [np.array([[800,285],[1456,285],[1456,470],[1100,470],[1080,440],[700,440],[700,330]])], 255)
cv2.rectangle(mask, (640,520), (840,720), 0, -1)  # character + shadow
bh = cv2.morphologyEx(g, cv2.MORPH_BLACKHAT, cv2.getStructuringElement(cv2.MORPH_RECT, (9,9)))
bh = cv2.bitwise_and(bh, mask)
_, bw = cv2.threshold(bh, 25, 255, cv2.THRESH_BINARY)
lines = cv2.HoughLinesP(bw, 1, np.pi/360, 40, minLineLength=45, maxLineGap=4)
L = np.asarray(lines).reshape(-1,4).astype(float)
ang = np.degrees(np.arctan2(L[:,3]-L[:,1], L[:,2]-L[:,0])) % 180
def vp(sel):
    S = L[sel]; hs = []
    for x1,y1,x2,y2 in S:
        l = np.cross([x1,y1,1],[x2,y2,1]); hs.append(l/np.linalg.norm(l[:2]))
    A = np.array(hs)
    best=None
    rng=np.random.default_rng(0)
    for _ in range(2000):
        i,j = rng.choice(len(A),2,replace=False); p=np.cross(A[i],A[j])
        if abs(p[2])<1e-9: continue
        p=p/p[2]; d=np.abs(A@p); inl=d<3.0
        if best is None or inl.sum()>best[0]: best=(inl.sum(),inl)
    inl=best[1]; Ai=A[inl]
    _,_,V=np.linalg.svd(Ai); p=V[-1]; p=p/p[2]
    return p[:2], inl.sum(), len(A)
# families: "across" (roughly horizontal) and "depth" (steep)
across = (ang < 30) | (ang > 150)
depth = (ang > 40) & (ang < 140)
v1,n1,t1 = vp(across); v2,n2,t2 = vp(depth)
c = np.array([W/2, H/2])
f2 = -np.dot(v1-c, v2-c)
out=[f"lines across {n1}/{t1} VP {v1.round(0)}; depth {n2}/{t2} VP {v2.round(0)}"]
if f2>0:
    f=np.sqrt(f2); hfov=2*np.degrees(np.arctan(W/2/f)); vfov=2*np.degrees(np.arctan(H/2/f))
    # horizon = line through v1,v2; pitch from distance of centre to horizon
    n=np.array([v2[1]-v1[1], v1[0]-v2[0]]); n/=np.linalg.norm(n); dist=abs(np.dot(c-v1,n))
    pitch=np.degrees(np.arctan(dist/f))
    out.append(f"f={f:.0f}px HFOV={hfov:.1f} VFOV={vfov:.1f} pitch_down={pitch:.1f}")
else: out.append("no real focal (VPs not orthogonal-consistent)")
dbg=img.copy()
for (x1,y1,x2,y2),a in zip(L.astype(int),ang): cv2.line(dbg,(x1,y1),(x2,y2),(0,0,255) if ((a<30)|(a>150)) else (0,255,0),2)
cv2.imwrite(sys.argv[2], dbg)
print("\n".join(out))
# zero-roll refinement: horizon y = depth VP y; across VP = median intersection of across lines with that horizon
yh = v2[1]; xs=[]
for x1,y1,x2,y2 in L[across]:
    if abs(y2-y1)<1e-6: continue
    xs.append(x1 + (yh-y1)*(x2-x1)/(y2-y1))
xs=np.array(xs); 
for side in (xs[xs<c[0]], xs[xs>c[0]]):
    if len(side)==0: continue
    vx=np.median(side); f2=-(vx-c[0])*(v2[0]-c[0])-(yh-c[1])**2
    if f2>0:
        f=np.sqrt(f2); print(f"zero-roll: n={len(side)} acrossVPx={vx:.0f} spread(IQR)={np.subtract(*np.percentile(side,[75,25])):.0f} f={f:.0f} HFOV={2*np.degrees(np.arctan(c[0]/f)):.1f} pitch={np.degrees(np.arctan((c[1]-yh)/f)):.1f}")
    else: print(f"zero-roll side n={len(side)} vx={vx:.0f}: no real f")
