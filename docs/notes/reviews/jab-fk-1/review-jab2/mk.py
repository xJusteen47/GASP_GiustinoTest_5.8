import sys
from PIL import Image, ImageDraw
cap,out,frames,views,crop,size=sys.argv[1],sys.argv[2],[int(x) for x in sys.argv[3].split(',')],sys.argv[4].split(','),[int(x) for x in sys.argv[5].split(',')],int(sys.argv[6])
S=Image.new('RGB',(len(frames)*size,len(views)*size))
for j,v in enumerate(views):
    for i,f in enumerate(frames):
        im=Image.open(f'{cap}/jab1_jab2_faf/{v}/f{f:03d}.png').convert('RGB').crop(crop).resize((size,size),Image.LANCZOS)
        d=ImageDraw.Draw(im); d.rectangle([0,0,90,12],fill=(0,0,0)); d.text((2,0),f'{v} v{f} a{f-33}',fill=(255,255,0))
        S.paste(im,(i*size,j*size))
S.save(out); print(out,S.size)
