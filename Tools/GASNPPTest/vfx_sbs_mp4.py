"""Owner side-by-side video (smoke redo A1): a 2x2 grid of frame sequences at 1920x1080, H.264, 60 fps, played at real speed, then again
at quarter speed. Each panel: a folder of PNG/JPG frames, the first frame, a frame step (1 = 60 fps source, 0.5 = 30 fps source shown at
60), a crop zoom and centre y (as vfx_owner_mp4.py), and a label burnt in. Panels shorter than the clip hold their last frame.
    python vfx_sbs_mp4.py <out.mp4> <frames per pass> <panel spec> x4
    panel spec = "label|folder|pattern|first|step|zoom|cy"      pattern e.g. f{:03d}.png or d{:04d}.png
Uses the ffmpeg binary of the imageio-ffmpeg pip package. Prints one line."""
import os
import subprocess
import sys
import tempfile

import imageio_ffmpeg
from PIL import Image, ImageDraw, ImageFont

W, H = 960, 540
out, n = sys.argv[1], int(sys.argv[2])
specs = [s.split("|") for s in sys.argv[3:7]]
try:
    font = ImageFont.truetype("arial.ttf", 30)
except OSError:
    font = ImageFont.load_default()


def panel(spec, i):
    label, folder, pattern, first, step, zoom, cy = spec
    k = int(first) + int(i * float(step))
    p = os.path.join(folder, pattern.format(k))
    while not os.path.exists(p) and k > int(first):
        k -= 1
        p = os.path.join(folder, pattern.format(k))
    img = Image.open(p).convert("RGB")
    w, h = img.size
    z = float(zoom)
    cw, ch = round(min(w, h * 16 / 9) / z), round(min(h, w * 9 / 16) / z)
    top = min(max(round(h * float(cy) - ch / 2), 0), h - ch)
    img = img.crop(((w - cw) // 2, top, (w - cw) // 2 + cw, top + ch)).resize((W, H), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, 20 + d.textlength(label, font=font), 44], fill=(0, 0, 0))
    d.text((10, 5), label, fill=(255, 255, 255), font=font)
    return img


tmp = tempfile.mkdtemp(prefix="gstn_sbs_")
frames = [i for i in range(n)] + [i // 4 for i in range(n * 4)]  # real speed, then quarter speed
font_s = font
for j, i in enumerate(frames):
    canvas = Image.new("RGB", (W * 2, H * 2))
    for q, spec in enumerate(specs):
        canvas.paste(panel(spec, i), ((q % 2) * W, (q // 2) * H))
    if j >= n:
        d = ImageDraw.Draw(canvas)
        d.rectangle([W - 70, H - 22, W + 70, H + 22], fill=(0, 0, 0))
        d.text((W - 55, H - 18), "x 0.25", fill=(255, 220, 80), font=font_s)
    canvas.save(os.path.join(tmp, f"{j:04d}.png"))
ff = imageio_ffmpeg.get_ffmpeg_exe()
r = subprocess.run([ff, "-y", "-loglevel", "error", "-framerate", "60", "-i", os.path.join(tmp, "%04d.png"), "-c:v", "libx264", "-pix_fmt",
                    "yuv420p", "-crf", "18", "-movflags", "+faststart", out], capture_output=True, text=True)
print(("ok " if r.returncode == 0 else "ffmpeg failed " + r.stderr[-200:]) + f" {out} {len(frames)} frames")
