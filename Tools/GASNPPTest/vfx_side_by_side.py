"""Owner video of a VFX round (no ffmpeg on the laptop): one animated WebP, light floor | dark floor side by side, game camera, real speed
(captures are 60 fps), one window per sequence with its name burnt in.
    python vfx_side_by_side.py <out.webp> <cell px> <light_root>:<dark_root>:<sequence>:<from>:<to> [...]
Prints one line."""
import os
import sys

from PIL import Image, ImageDraw

out, cell = sys.argv[1], int(sys.argv[2])
frames = []
for spec in sys.argv[3:]:
    light, dark, seq, a, b = spec.split(":") if spec.count(":") == 4 else spec.rsplit(":", 4)
    for f in range(int(a), int(b) + 1):
        paths = [os.path.join(root, seq, "game", f"f{f:03d}.png") for root in (light, dark)]
        if not all(os.path.exists(p) for p in paths):
            continue
        img = Image.new("RGB", (cell * 2, cell))
        for i, p in enumerate(paths):
            img.paste(Image.open(p).convert("RGB").resize((cell, cell)), (i * cell, 0))
        d = ImageDraw.Draw(img)
        d.rectangle([0, 0, 200, 14], fill=(0, 0, 0))
        d.text((3, 1), f"{seq} v{f}  light | dark", fill=(255, 255, 0))
        frames.append(img)
frames[0].save(out, save_all=True, append_images=frames[1:], duration=17, loop=0, quality=70)
print(f"{out}: {len(frames)} frames, {os.path.getsize(out) // 1024} KB")
