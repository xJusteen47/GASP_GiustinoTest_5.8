import subprocess,tempfile,sys
from PIL import Image,ImageDraw,ImageFont
R='/home/user/GASP_GiustinoTest_5.8'; S=sys.argv[1]
B=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',16); F=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',13)
d=tempfile.mkdtemp(); subprocess.run(['ffmpeg','-v','error','-i',R+'/docs/notes/reviews/cp5b-jab-chain/jab_jab1_jab2_faf.mp4','-fps_mode','passthrough',d+'/%04d.png'],check=True)
V={'behind':(0,360),'az0':(0,0)}
fr=list(range(23,33)); cw=150; W=10+len(fr)*(cw+4); img=Image.new('RGB',(W,560),(250,250,250)); dr=ImageDraw.Draw(img)
dr.text((10,6),'JAB 1 oggi (cattura cp5b, a vuoto): lo schiaffo si ferma sul contatto e resta fermo 6 frame',fill=(0,0,0),font=B)
step={23:'18.6',24:'41.0',25:'75.8',26:'',27:'3.6',28:'3.1',29:'0.4',30:'1.0',31:'1.0',32:'0.4'}
# card frames: a = v-21 ; hand steps from round2 notes f3->f4 .. f12->f13
steps={'f3':18.6,'f4':41.0,'f5':75.8}
for row,(view,lab) in enumerate([('behind','da dietro (camera di gioco)'),('az0','di fronte')]):
    y=32+row*250; dr.text((10,y),lab,fill=(60,60,60),font=F)
    for i,v in enumerate(fr):
        im=Image.open(d+'/%04d.png'%(v-14+1)); x0,y0=V[view]; im=im.crop((x0+75,y0+40,x0+75+210,y0+40+210)).resize((cw,cw)); x=10+i*(cw+4)
        img.paste(im,(x,y+18)); a=v-21; col=(200,0,0) if a==6 else ((0,120,0) if a<6 else (120,120,120))
        dr.rectangle((x,y+18,x+cw-1,y+18+cw-1),outline=col,width=3)
        dr.text((x+3,y+18+cw+3),'f%d%s'%(a,' CONTATTO' if a==6 else (' fermo' if 7<=a<=12 else '')),fill=col,font=F)
sp=['>f2 43','>f3 19','>f4 19','>f5 41','>f6 76','>f7 3.6','>f8 3.1','>f9 0.4','>f10 1.0','>f11 1.0']
y=532; dr.text((10,y-18),'cm percorsi dalla mano per frame (scheda r2, overlord_jab_v3_round2_notes.md):',fill=(0,0,0),font=F)
for i,t in enumerate(sp): dr.text((10+i*(cw+4),y),t,fill=(0,0,0),font=F)
img.save(S+'/jab_confronto.png'); print('ok')
