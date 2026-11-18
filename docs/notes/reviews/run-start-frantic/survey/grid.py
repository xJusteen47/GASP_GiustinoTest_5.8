import json,sys,os,glob
from PIL import Image,ImageDraw
r=sys.argv[1]
for fj in glob.glob(os.path.join(r,'*_frames.json')):
    tag=os.path.basename(fj)[:-12]; fr=json.load(open(fj))
    g=Image.new('RGB',(2048,1024),'white'); d=ImageDraw.Draw(g)
    for i,(p,f) in enumerate(fr):
        im=Image.open(p).convert('RGB'); x,y=(i%4)*512,(i//4)*512; g.paste(im,(x,y)); d.text((x+8,y+8),f'{tag} f{f}',fill='black')
    g.save(os.path.join(r,tag+'_grid.png'))
print('grids ok')
