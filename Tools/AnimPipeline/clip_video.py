"""Review video and contact sheet of any clip on the real mesh (editor lit captures).

    python clip_video.py <anim asset path> <number of frames> --out=<mp4> [--label=<text>] [--fps=12]

Captures three views per frame (right side, back, 3/4 front), renders an mp4 and a 3-column contact sheet.
Prints the output path only.
"""
import os, sys, time, shutil, tempfile, subprocess
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue

RAW = os.path.join(HERE, "sheets", "raw")
VIEWS = [(270, 8.0), (180, 14.0), (315, 12.0)]


def main():
    anim = sys.argv[1]
    n_frames = int(sys.argv[2])
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[3:] if a.startswith("--"))
    out = opt.get("out") or f"{anim.rsplit('/', 1)[-1]}.mp4"
    name = opt.get("label") or anim.rsplit("/", 1)[-1]
    fps = int(opt.get("fps", 12))
    st = ue.py("setup")
    if isinstance(st, dict) and st.get("error"):
        # a build leaves the open level dirty (temp actor) and setup never switches away from a dirty level
        sys.exit(f"capture setup: {st}  (drop the dirty level first: EditorLoadingAndSavingUtils.new_blank_map(False))")
    tmpdir = tempfile.mkdtemp()
    try:
        sheet_frames = []
        for f in range(n_frames):
            for _ in range(2):      # the preview mesh takes the pose one tick late: set it twice
                ue.py("set_frame", anim_path=anim, frame=f, hide_arm=False)
                time.sleep(0.15)
            frame_im = Image.new("RGB", (1152, 384))
            for c, (az, el) in enumerate(VIEWS):
                tag = f"cv_{name}_{f}_{c}"
                ue.py("capture", kind="lit", az=az, el=el, tag=tag, size=384, dist=430.0, fov=40.0, center=[0, 0, 70])
                p = os.path.join(RAW, tag + ".rgb")
                im = Image.fromarray(np.fromfile(p, dtype=np.uint8).reshape(384, 384, 3))
                os.remove(p)
                frame_im.paste(im, (c * 384, 0))
            d = ImageDraw.Draw(frame_im)
            d.rectangle([0, 0, 1152, 14], fill=(20, 20, 20))
            d.text((4, 1), f"{name} f{f} | right | back | 3/4 front", fill=(255, 255, 255))
            frame_im.save(os.path.join(tmpdir, f"f_{f:04d}.png"))
            sheet_frames.append(frame_im.resize((576, 192)))
        out_dir = os.path.dirname(os.path.abspath(out))
        if out_dir:
            os.makedirs(out_dir, exist_ok=True)
        cmd = ["ffmpeg", "-y", "-framerate", str(fps), "-i", "f_%04d.png", "-c:v", "libx264", "-pix_fmt", "yuv420p", os.path.abspath(out)]
        subprocess.run(cmd, cwd=tmpdir, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        sheet_path = (out[:-4] if out.lower().endswith(".mp4") else out) + "_sheet.png"
        cols = 3
        rows = max(1, (n_frames + cols - 1) // cols)
        sheet = Image.new("RGB", (cols * 576, rows * 192), (12, 12, 12))
        for i, sim in enumerate(sheet_frames):
            sheet.paste(sim, ((i % cols) * 576, (i // cols) * 192))
        sheet.save(sheet_path)
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)
    print(out)


if __name__ == "__main__":
    main()
