#!/usr/bin/env python3
"""Extract frames from a capture mp4 and compose ONE labelled grid image.

CLI:
  rv_frames.py VIDEO --frames 18-34 [--frames 21,28,29 ...] --out GRID.jpg
      [--crop x,y,w,h] [--cols 8] [--cell 360] [--vs BASE.mp4] [--diff]
      [--thresh 20] [--mark 28,29]
Frames are 0-based decoded frame numbers. --diff writes GRID.json with the
changed-pixel count between each frame and the previous one. Exit 2 = bad input.

Examples:
  rv_frames.py cap.mp4 --frames 18-34 --crop 0,0,720,720 --out g.jpg --diff --mark 28,29
  rv_frames.py var.mp4 --frames 0-15 --vs base.mp4 --out cmp.jpg
"""
import argparse, glob, json, os, subprocess, sys, tempfile
from PIL import Image, ImageChops, ImageDraw, ImageFont

MAXSIDE, MAXCELLS = 1568, 48


def die(msg):
    print("error: " + msg)
    sys.exit(2)


def parse_frames(specs):
    out = set()
    for spec in specs:
        for part in spec.split(","):
            try:
                a, _, b = part.strip().partition("-")
                out.update(range(int(a), int(b or a) + 1))
            except ValueError:
                die(f"bad frame spec '{part}'")
    return sorted(out)


def nframes(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
                        "-show_entries", "stream=nb_read_frames", "-of", "csv=p=0", path],
                       capture_output=True, text=True)
    try:
        return int(r.stdout.strip().split()[0])
    except (ValueError, IndexError):
        die(f"cannot read video '{path}'")


def extract(path, frames, crop, tmp, tag):
    """Decode the given frame numbers; return {n: PIL image}. Missing ones are absent."""
    if not frames:
        return {}
    sel = "+".join(f"eq(n\\,{n})" for n in frames) if len(frames) < 40 else \
          "+".join(f"between(n\\,{a}\\,{b})" for a, b in runs(frames))
    vf = f"select='{sel}'" + (f",crop={crop[2]}:{crop[3]}:{crop[0]}:{crop[1]}" if crop else "")
    d = os.path.join(tmp, tag)
    os.makedirs(d)
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-vf", vf, "-fps_mode", "passthrough",
                        os.path.join(d, "%06d.png")], capture_output=True, text=True)
    files = sorted(glob.glob(os.path.join(d, "*.png")))
    if r.returncode and not files:
        die("ffmpeg failed: " + (r.stderr.strip().splitlines() or ["?"])[-1])
    return {n: Image.open(f).convert("RGB") for n, f in zip(frames, files)}


def runs(frames):
    out, a, p = [], frames[0], frames[0]
    for n in frames[1:]:
        if n != p + 1:
            out.append((a, p)); a = n
        p = n
    return out + [(a, p)]


def changed(a, b, thresh):
    d = ImageChops.difference(a, b)
    r, g, bl = d.split()
    m = ImageChops.lighter(ImageChops.lighter(r, g), bl)
    return sum(m.histogram()[thresh + 1:])


def diffs(imgs, frames, thresh):
    return [{"v": n, "changed_px": (changed(imgs[n - 1], imgs[n], thresh)
                                     if n > 0 and n in imgs and n - 1 in imgs else None)} for n in frames]


def main():
    ap = argparse.ArgumentParser(description="Compose a labelled frame grid from an mp4.")
    ap.add_argument("video")
    ap.add_argument("--frames", action="append", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--crop")
    ap.add_argument("--cols", type=int, default=8)
    ap.add_argument("--cell", type=int, default=360)
    ap.add_argument("--vs")
    ap.add_argument("--diff", action="store_true")
    ap.add_argument("--thresh", type=int, default=20)
    ap.add_argument("--mark", default="")
    try:
        a = ap.parse_args()
    except SystemExit as e:
        sys.exit(2 if e.code else 0)
    frames = parse_frames(a.frames)
    marks = set(parse_frames([a.mark])) if a.mark else set()
    crop = None
    if a.crop:
        try:
            crop = [int(x) for x in a.crop.split(",")]
            assert len(crop) == 4 and crop[2] > 0 and crop[3] > 0
        except (ValueError, AssertionError):
            die("--crop must be x,y,w,h")
    for p in [a.video] + ([a.vs] if a.vs else []):
        if not os.path.isfile(p):
            die(f"file not found: {p}")
    if len(frames) > MAXCELLS:
        die(f"{len(frames)} frames requested, max {MAXCELLS} per video: split the range")
    total = nframes(a.video)
    if frames[-1] >= total:
        die(f"frame {frames[-1]} beyond end ({total} frames, last is {total - 1})")
    need = sorted(set(frames) | ({n - 1 for n in frames if n > 0} if a.diff else set()))
    with tempfile.TemporaryDirectory() as tmp:
        var = extract(a.video, need, crop, tmp, "var")
        base = {}
        if a.vs:
            bt = nframes(a.vs)
            base = extract(a.vs, [n for n in need if n < bt], crop, tmp, "base")
    if len(var) != len(need):
        die("decoded fewer frames than expected")
    cw = a.cell
    ch = round(cw * next(iter(var.values())).height / next(iter(var.values())).width)
    rowsets = [frames[i:i + a.cols] for i in range(0, len(frames), a.cols)]
    nrows = len(rowsets) * (2 if a.vs else 1)
    cols = min(a.cols, len(frames))
    lw = 0.0

    def dims(strip, lw):
        return lw + cols * cw, nrows * (ch + strip)

    # label size is chosen so text is ~18 px tall after the final downscale
    W, H = dims(0, 0)
    s = min(1.0, MAXSIDE / max(W, H))
    for _ in range(3):
        fs = max(14, round(18 / s))
        strip = round(fs * 1.4)
        lw = round(fs * 2.6) if a.vs else 0
        W, H = dims(strip, lw)
        s = min(1.0, MAXSIDE / max(W, H))
    font = ImageFont.load_default(size=fs)
    img = Image.new("RGB", (int(W), int(H)), (30, 30, 30))
    dr = ImageDraw.Draw(img)
    for ri, row in enumerate(rowsets):
        for k, (src, name) in enumerate(([(var, "var"), (base, "base")] if a.vs else [(var, "")])):
            y = (ri * (2 if a.vs else 1) + k) * (ch + strip)
            if a.vs:
                dr.text((6, y + strip), name, fill=(255, 255, 255), font=font)
            for ci, n in enumerate(row):
                x = lw + ci * cw
                im = src.get(n)
                cell = Image.new("RGB", (cw, ch), (110, 110, 110))
                if im:
                    cell = im.resize((cw, ch), Image.LANCZOS)
                img.paste(cell, (int(x), y + strip))
                mk = n in marks
                label = ("base " if name == "base" else "") + f"v{n}" + (" *" if mk else "")
                dr.text((x + 4, y + (strip - fs) // 2 - 1), label, fill=(255, 90, 90) if mk else (255, 255, 255), font=font)
                if mk:
                    bw = max(3, round(3 / s))
                    dr.rectangle([x, y, x + cw - 1, y + ch + strip - 1], outline=(255, 0, 0), width=bw)
    if s < 1:
        img = img.resize((round(W * s), round(H * s)), Image.LANCZOS)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    img.save(a.out, "JPEG", quality=88)
    print(a.out)
    print(f"{len(frames)} cells x {2 if a.vs else 1} rows, {img.width}x{img.height}px")
    if a.diff:
        res = {"video": a.video, "crop": crop, "thresh": a.thresh, "frames": diffs(var, frames, a.thresh)}
        if a.vs:
            res["base"] = diffs(base, frames, a.thresh)
        jp = os.path.splitext(a.out)[0] + ".json"
        with open(jp, "w") as f:
            json.dump(res, f)
        vals = sorted((d["changed_px"], d["v"]) for d in res["frames"] if d["changed_px"] is not None)
        fmt = lambda L: " ".join(f"v{v}={k}" for k, v in L)
        print(jp)
        print("min " + fmt(vals[:3]))
        print("max " + fmt(vals[-3:][::-1]))


if __name__ == "__main__":
    main()
