"""Owner video of a VFX capture as H.264 mp4 (phone friendly, 2026-11-19: the WebP was too small to read).
Uses the ffmpeg binary of the imageio-ffmpeg pip package (the laptop has no ffmpeg on PATH). Frames are the 60 fps captures of
pie_capture_vfx.py run with -Wide 1920x1080, so the clip plays at real speed. A label (sequence, floor, view) is burnt into the top left.
    python vfx_owner_mp4.py <out.mp4> <capture root> <sequence> <view> <from> <to> <label> [zoom] [centre y]
zoom > 1 crops the frame around (0.5, centre y) and scales it back to full size: a closer framing that keeps the feet in frame (the
'close' view narrows the FOV around the screen centre and cut the fighter's legs in 16:9).
Prints one line."""
import os
import subprocess
import sys
import tempfile

import imageio_ffmpeg
from PIL import Image, ImageDraw, ImageFont

out, root, seq, view, a, b, label = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), int(sys.argv[6]), sys.argv[7]
zoom = float(sys.argv[8]) if len(sys.argv) > 8 else 1.0
cy = float(sys.argv[9]) if len(sys.argv) > 9 else 0.5
src = os.path.join(root, seq, view)
try:
    font = ImageFont.truetype('arial.ttf', 40)
except OSError:
    font = ImageFont.load_default()
tmp = tempfile.mkdtemp(prefix='gstn_mp4_')
n = 0
for f in range(a, b + 1):
    p = os.path.join(src, f'f{f:03d}.png')
    if not os.path.exists(p):
        continue
    img = Image.open(p).convert('RGB')
    if zoom > 1.0:
        w, h = img.size
        cw, ch = round(w / zoom), round(h / zoom)
        top = min(max(round(h * cy - ch / 2), 0), h - ch)
        img = img.crop(((w - cw) // 2, top, (w - cw) // 2 + cw, top + ch)).resize((w, h), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 24 + d.textlength(label, font=font), 56], fill=(0, 0, 0))
    d.text((12, 6), label, fill=(255, 255, 255), font=font)
    img.save(os.path.join(tmp, f'{n:04d}.png'))
    n += 1
cmd = [imageio_ffmpeg.get_ffmpeg_exe(), '-y', '-loglevel', 'error', '-framerate', '60', '-i', os.path.join(tmp, '%04d.png'),
       '-c:v', 'libx264', '-preset', 'slow', '-crf', '16', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', out]
subprocess.run(cmd, check=True)
print(f'{out}: {n} frames, {os.path.getsize(out) // 1024} KB')
