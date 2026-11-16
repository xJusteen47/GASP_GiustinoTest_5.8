"""grid.py OUT cols cell SPEC...   SPEC = dir:first_v:view:frames[:crop]  frames '19-63' or '19,21,30-33'; crop 'x,y,w,h' inside the 360 view
Several SPECs are interleaved row by row (row of spec1, row of spec2 ...) when given with --rows, else concatenated."""
import sys,os

def frames_dir(p):
    """p = folder of %04d.png, or an mp4 (decoded to a temp folder)."""
    import os,subprocess,tempfile
    if os.path.isdir(p): return p
    d=tempfile.mkdtemp(); subprocess.run(['ffmpeg','-v','error','-i',p,'-fps_mode','passthrough',os.path.join(d,'%04d.png')],check=True); return d
from PIL import Image,ImageDraw,ImageFont
VIEWS={'az0':(0,0),'front34':(360,0),'side':(720,0),'game':(1080,0),'behind':(0,360),'az270':(360,360),'az315':(720,360),'label':(1080,360)}
args=[a for a in sys.argv[1:] if not a.startswith('--')]
out,cols,cell=args[0],int(args[1]),int(args[2])
marks=set()
for a in sys.argv:
    if a.startswith('--mark='): marks=set(int(x) for x in a[7:].split(','))
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',14)
def frames(s):
    o=[]
    for p in s.split(','):
        a,_,b=p.partition('-'); o+=range(int(a),int(b or a)+1)
    return o
seqs=[]
for sp in args[3:]:
    p=sp.split(':'); d,first,view,fr=frames_dir(p[0]),int(p[1]),p[2],frames(p[3])
    crop=[int(x) for x in p[4].split(',')] if len(p)>4 else [0,0,360,360]
    tag=p[5] if len(p)>5 else ''
    cells=[]
    for v in fr:
        f=os.path.join(d,'%04d.png'%(v-first+1))
        if not os.path.exists(f): continue
        vx,vy=VIEWS[view]
        im=Image.open(f).crop((vx+crop[0],vy+crop[1],vx+crop[0]+crop[2],vy+crop[1]+crop[3]))
        ch=int(cell*crop[3]/crop[2]); im=im.resize((cell,ch),Image.LANCZOS)
        dr=ImageDraw.Draw(im); t='%s%d'%(tag,v)
        dr.rectangle((0,ch-18,dr.textlength(t,font=font)+6,ch),fill=(0,0,0)); dr.text((3,ch-17),t,fill=(255,255,0),font=font)
        if v in marks: dr.rectangle((0,0,cell-1,ch-1),outline=(255,0,0),width=3)
        cells.append(im)
    seqs.append(cells)
rows=[]
if '--rows' in sys.argv:
    n=max(len(s) for s in seqs)
    for i in range(0,n,cols):
        for s in seqs:
            if s[i:i+cols]: rows.append(s[i:i+cols])
else:
    allc=[c for s in seqs for c in s]
    rows=[allc[i:i+cols] for i in range(0,len(allc),cols)]
ch=rows[0][0].height
W,H=cols*cell,len(rows)*ch
g=Image.new('RGB',(W,H),(30,30,30))
for r,row in enumerate(rows):
    for c,im in enumerate(row): g.paste(im,(c*cell,r*ch))
g.save(out,quality=88); print(out,W,H,sum(len(r) for r in rows),'cells')
