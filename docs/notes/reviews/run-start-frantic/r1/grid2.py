import sys,os
from PIL import Image,ImageDraw
d,view,out=sys.argv[1],sys.argv[2],sys.argv[3]
fr=list(range(1,28,2)); W=384
g=Image.new('RGB',(W*7,W*2),'white'); dr=ImageDraw.Draw(g)
for i,f in enumerate(fr):
    im=Image.open(os.path.join(d,f'{view}_{f:02d}.png')).convert('RGB').resize((W,W))
    x,y=(i%7)*W,(i//7)*W; g.paste(im,(x,y)); dr.text((x+6,y+6),f'{view} f{f}'+(' (in place)' if f<=10 else ''),fill='black')
g.save(out); print('ok',out)
