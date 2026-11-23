import json,subprocess,tempfile,os,sys
from PIL import Image,ImageDraw,ImageFont
R='/home/user/GASP_GiustinoTest_5.8'; S=sys.argv[1]
B=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',16); F=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',14)
mp4=R+'/docs/notes/reviews/cp5b-dropkick/dropkick_dropkick_whiff.mp4'; d=tempfile.mkdtemp()
subprocess.run(['ffmpeg','-v','error','-i',mp4,'-fps_mode','passthrough',d+'/%04d.png'],check=True)
V={'side':(720,0),'game':(1080,0),'behind':(0,360)}
def cell(v,view,crop=(30,20,300,300),size=300):
    im=Image.open(d+'/%04d.png'%(v-36+1)); x,y=V[view]; return im.crop((x+crop[0],y+crop[1],x+crop[0]+crop[2],y+crop[1]+crop[3])).resize((size,size))
W=1240; H=900; img=Image.new('RGB',(W,H),(250,250,250)); dr=ImageDraw.Draw(img)
dr.text((10,8),'DROPKICK — oggi in gioco (cattura cp5b, a vuoto)',fill=(0,0,0),font=B)
for i,(v,view,t) in enumerate([(58,'side','v58 lato: primo frame attivo'),(58,'game','v58 camera di gioco'),(80,'side','v80 lato: tenuta in aria'),(80,'game','v80 camera di gioco')]):
    img.paste(cell(v,view),(10+i*307,34)); dr.text((14+i*307,338),t,fill=(0,0,0),font=F)
y0=372; dr.text((10,y0),'Gambe nel sistema del bacino (numeri delle schede, non render): Antigravity r0 vs oggi',fill=(0,0,0),font=B)
c=lambda f:json.load(open(R+'/Tools/AnimPipeline/cards/overlord/dropkick/'+f))['author']['poses']['0']
A=c('overlord_dropkick_loop.r0.backup.json'); N=c('overlord_dropkick_loop.json')
def panel(ox,title,pose,col,mode):
    dr.rectangle((ox,y0+28,ox+290,y0+470),outline=(180,180,180)); dr.text((ox+6,y0+32),title,fill=col,font=B)
    cx,cy=ox+145,y0+140; s=3.0
    for side,hx in (('leg_l',10),('leg_r',-10)):
        f=pose[side]['foot']
        if mode=='front': p=(cx-f[0]*s,cy-f[2]*s); h=(cx-hx*s,cy)
        else: p=(cx+f[1]*s,cy-f[2]*s); h=(cx,cy)
        dr.line([h,p],fill=col if side=='leg_l' else (230,140,0),width=6); dr.ellipse((p[0]-6,p[1]-6,p[0]+6,p[1]+6),fill=(0,0,0))
    dr.ellipse((cx-9,cy-9,cx+9,cy+9),fill=(90,90,90)); dr.text((ox+6,y0+440),('vista da dietro' if mode=='front' else 'vista di lato (avanti = destra)'),fill=(60,60,60),font=F)
panel(10,'Antigravity: da dietro',A,(40,90,200),'front'); panel(320,'Antigravity: di lato',A,(40,90,200),'side')
panel(630,'Oggi: da dietro',N,(200,40,40),'front'); panel(940,'Oggi: di lato',N,(200,40,40),'side')
img.save(S+'/dropkick_confronto.png'); print('ok')
