import cv2, numpy as np, sys
img=cv2.imread(sys.argv[1]); H,W=img.shape[:2]
x0,y0,x1,y1=[int(v) for v in sys.argv[2].split(',')]
g=cv2.cvtColor(img,cv2.COLOR_BGR2GRAY); e=cv2.Canny(g,50,150)
m=np.zeros_like(g); m[y0:y1,x0:x1]=255; e=cv2.bitwise_and(e,m)
L=np.asarray(cv2.HoughLinesP(e,1,np.pi/720,40,minLineLength=60,maxLineGap=6)).reshape(-1,4).astype(float)
ang=np.degrees(np.arctan2(L[:,3]-L[:,1],L[:,2]-L[:,0]))%180
S=L[(ang>75)&(ang<105)]
# report each line: x at mid, tilt (dx/dy)
rows=[]
for x1_,y1_,x2_,y2_ in S:
    if y2_<y1_: x1_,y1_,x2_,y2_=x2_,y2_,x1_,y1_
    rows.append(((x1_+x2_)/2,(y1_+y2_)/2,(x2_-x1_)/(y2_-y1_),y2_-y1_))
rows.sort()
for r in rows: print("x=%.0f y=%.0f dxdy=%+.3f len=%.0f"%r)
# fit: for vertical VP at (cx, yv) with zero roll, dx/dy = (cx - x)/(yv - y)  -> yv = y + (cx-x)/dxdy
cx=W/2; ests=[r[1]+(cx-r[0])/r[2] for r in rows if abs(r[2])>0.005 and abs(cx-r[0])>150]
print("n=%d yv median=%.0f IQR=%s"%(len(ests), np.median(ests) if ests else -1, np.percentile(ests,[25,75]).round(0) if ests else '-'))
dbg=img.copy()
for x1_,y1_,x2_,y2_ in S.astype(int): cv2.line(dbg,(x1_,y1_),(x2_,y2_),(0,0,255),3)
cv2.imwrite(sys.argv[3],cv2.resize(dbg,(W//2,H//2)))
