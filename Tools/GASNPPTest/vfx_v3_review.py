"""VFX v3 review pack (rule 10 + owner's v3 rule: compare with the Astro Bot captures FIRST, real speed, light and dark).

  python make_v3_review.py <capture root with light/ and dark/> <out dir> <ref dir>

For every backdrop and sequence: <bd>_<seq>_game.mp4 (game camera, 60 fps = real speed) and <bd>_<seq>_close.mp4; one side-by-side sheet
per sequence: the owner's reference on the left, 8 of our game-camera frames on the right (4x2, frame numbers burnt in). Hit sequences:
the 8 frames start one frame before the brightest frame of the close view after the attack (the star), then +1 +2 +4 +6 +9 +13 +18.
Others: 8 frames spread over the window after the input. Prints one line.
"""
import os
import re
import shutil
import subprocess
import sys

from PIL import Image, ImageChops, ImageDraw

CAP, OUT, REF = sys.argv[1], sys.argv[2], sys.argv[3]
REFS = {"hit": "astro_1.png", "ground_bounce": "astro_3.png", "wall": "astro_3.png", "jump": "astro_3.png", "run_skid": "astro_3.png",
        "sprint": "astro_2.png"}
PRE = 20
H = 720


def ref_for(seq):
    for key, name in REFS.items():
        if seq.startswith(key):
            return name
    return "astro_1.png"


def inputs(log):
    """{sequence: first input frame (v)} from capture.log"""
    found = {}
    for line in open(log, encoding="utf-8", errors="replace"):
        m = re.search(r"(\w+): (attack|strong|jump|stick \(.*?\)) (?:on|from) v(\d+)", line)
        if m and m.group(1) not in found and "None" not in m.group(2):
            found[m.group(1)] = int(m.group(3))
    return found


def contacts(log):
    """{sequence: frame (v)} of the landing / floor contact logged by pie_capture_vfx._contact"""
    found = {}
    for line in open(log, encoding="utf-8", errors="replace"):
        m = re.search(r"(\w+): (?:land|bounce) on v(\d+)", line)
        if m and m.group(1) not in found:
            found[m.group(1)] = int(m.group(2))
    return found


def frame(folder, n):
    path = os.path.join(folder, f"f{n:03d}.png")
    return Image.open(path).convert("RGB") if os.path.exists(path) else None


def brightest(folder, start, end):
    best, best_n = -1, start
    for n in range(start, end):
        img = frame(folder, n)
        if img is None:
            break
        r, g, b = img.split()  # PIL only: the laptop python has no numpy
        score = ImageChops.darker(ImageChops.darker(r, g), b).point(lambda v: 255 if v >= 245 else 0).histogram()[255]
        if score > best:
            best, best_n = score, n
    return best_n


def sheet(bd, seq, picks, game):
    ref = Image.open(os.path.join(REF, ref_for(seq))).convert("RGB")
    ref = ref.resize((int(ref.width * H / ref.height), H))
    cell = H // 2
    grid = Image.new("RGB", (cell * 4, H), (0, 0, 0))
    for i, n in enumerate(picks):
        img = frame(game, n)
        if img is None:
            continue
        img = img.resize((cell, cell))
        ImageDraw.Draw(img).text((6, 4), f"v{n}", fill=(255, 0, 255))
        grid.paste(img, ((i % 4) * cell, (i // 4) * cell))
    out = Image.new("RGB", (ref.width + grid.width + 8, H), (255, 0, 255))
    out.paste(ref, (0, 0))
    out.paste(grid, (ref.width + 8, 0))
    out.save(os.path.join(OUT, f"{bd}_{seq}_vs_{ref_for(seq)[:-4]}.png"))


def mp4(folder, path):
    """60 fps mp4; without ffmpeg (the laptop) an animated WebP at 60 fps (17/17/16 ms) that a browser plays at real speed"""
    if shutil.which("ffmpeg") is None:
        names = sorted(f for f in os.listdir(folder) if re.fullmatch(r"f\d{3}\.png", f))
        imgs = [Image.open(os.path.join(folder, f)).convert("RGB") for f in names]
        if imgs:
            imgs[0].save(path[:-4] + ".webp", save_all=True, append_images=imgs[1:], loop=0, quality=80,
                         duration=[(17, 17, 16)[i % 3] for i in range(len(imgs))])
        return
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-framerate", "60", "-i", os.path.join(folder, "f%03d.png"), "-c:v", "libx264",
                    "-pix_fmt", "yuv420p", "-crf", "20", path], check=False)


os.makedirs(OUT, exist_ok=True)
made = 0
for bd in ("light", "dark"):
    root = os.path.join(CAP, bd)
    if not os.path.isdir(root):
        continue
    first = inputs(os.path.join(root, "capture.log"))
    touch = contacts(os.path.join(root, "capture.log"))
    # argv[4] "seq=v,...": a contact frame the driver missed (r7: the victim's z test never fired; the second GroundBounce event is 74
    # frames after the Hit event in r6 and r7, which r6's driver logged as v105)
    for item in (sys.argv[4].split(",") if len(sys.argv) > 4 else []):
        name, v = item.split("=")
        touch.setdefault(name, int(v))
    for seq in sorted(os.listdir(root)):
        # r3 laptop: the game/close cameras sit behind the attacker and the victim hides the hit; the side view shows it, so the
        # sheets use the side view and every view gets a video
        game, side = os.path.join(root, seq, "game"), os.path.join(root, seq, "side")
        if not os.path.isdir(side):
            continue
        count = len(os.listdir(side))
        for view in ("side", "game", "close"):
            if os.path.isdir(os.path.join(root, seq, view)):
                mp4(os.path.join(root, seq, view), os.path.join(OUT, f"{bd}_{seq}_{view}.mp4"))
        game = side
        start = first.get(seq, PRE)
        if seq.startswith("hit"):
            h = brightest(side, start, min(start + 30, count))
            picks = [h - 1, h, h + 1, h + 2, h + 4, h + 6, h + 9, h + 13]
        elif seq in touch:   # r5: half the sheet around the landing / floor contact (4 of 8 frames from contact - 1)
            t = touch[seq]
            picks = [start, start + (t - start) // 3, start + 2 * (t - start) // 3, t - 1, t, t + 2, t + 6, t + 14]
        else:
            span = max(8, count - start - 4)
            picks = [start + int(i * span / 8) for i in range(8)]
        sheet(bd, seq, [p for p in picks if 0 <= p < count], game)
        made += 1
print(f"{made} sequences -> {OUT}")
