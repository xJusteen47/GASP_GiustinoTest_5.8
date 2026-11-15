"""rv_diff.py CAPTURE.mp4 FIRST_V side|behind : changed pixels (threshold 20) between consecutive frames of one view, as v:count. FIRST_V = v of the first video frame (rows - video frames)."""

def frames_dir(p):
    """p = folder of %04d.png, or an mp4 (decoded to a temp folder)."""
    import os,subprocess,tempfile
    if os.path.isdir(p): return p
    d=tempfile.mkdtemp(); subprocess.run(['ffmpeg','-v','error','-i',p,'-fps_mode','passthrough',os.path.join(d,'%04d.png')],check=True); return d
import sys,glob,os
from PIL import Image,ImageChops
d,first=frames_dir(sys.argv[1]),int(sys.argv[2])   # first = v of first video frame
box={'side':(720,30,1080,360),'behind':(0,390,360,720)}[sys.argv[3]]
fs=sorted(glob.glob(d+'/*.png')); prev=None; out=[]
for i,f in enumerate(fs):
    im=Image.open(f).convert('L').crop(box)
    if prev is not None:
        df=ImageChops.difference(im,prev).point(lambda p:255 if p>20 else 0)
        out.append("%d:%d"%(first+i,sum(df.histogram()[255:])))
    prev=im
print(os.path.basename(sys.argv[1]),sys.argv[3],' '.join(out))
