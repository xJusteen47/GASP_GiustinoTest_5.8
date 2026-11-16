"""One review grid of any clip on the real mesh (editor lit captures): rows = frames, columns = views.

    python clip_grid.py <anim asset path> <f,f,...> --out=<png> [--views=6|4] [--cell=256] [--label=<text>] [--center=x,y,z]

For clips the move checks do not cover (reactions, get-ups: no hitbox, no strike limb), so `animpipe render` refuses them.
views 6 = front, right, back, left, top, bottom (owner's six-view rule); 4 = right side, back (the game camera looks at the
character's back), 3/4 front, top. The camera looks at --center (default 0,0,70: fits standing and lying poses).
Prints the output path only.
"""
import os, sys, time
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ue

RAW = os.path.join(HERE, "sheets", "raw")
VIEWS = {"6": [("front", 0, 10.0), ("right", 270, 10.0), ("back", 180, 10.0), ("left", 90, 10.0), ("top", 0, 88.0), ("bottom", 0, -88.0)],
         "4": [("right", 270, 8.0), ("back", 180, 14.0), ("3/4 front", 315, 12.0), ("top", 0, 88.0)]}


def main():
    anim = sys.argv[1]
    frames = [int(x) for x in sys.argv[2].split(",")]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[3:] if a.startswith("--"))
    views = VIEWS[opt.get("views", "6")]
    cell = int(opt.get("cell", 256))
    center = [float(x) for x in opt.get("center", "0,0,70").split(",")]
    name = opt.get("label") or anim.rsplit("/", 1)[-1]
    st = ue.py("setup")
    if isinstance(st, dict) and st.get("error"):
        # a build leaves the open level dirty (temp actor) and setup never switches away from a dirty level
        sys.exit(f"capture setup: {st}  (drop the dirty level first: EditorLoadingAndSavingUtils.new_blank_map(False))")
    grid =Image.new("RGB", (cell * len(views), cell * len(frames)), (12, 12, 12))
    for r, f in enumerate(frames):
        for _ in range(2):      # the preview mesh takes the pose one tick late: set it twice
            ue.py("set_frame", anim_path=anim, frame=f, hide_arm=False)
            time.sleep(0.15)
        for c, (vn, az, el) in enumerate(views):
            tag = f"cg_{name}_{f}_{c}"
            ue.py("capture", kind="lit", az=az, el=el, tag=tag, size=384, dist=430.0, fov=40.0, center=center)
            p = os.path.join(RAW, tag + ".rgb")
            im = Image.fromarray(np.fromfile(p, dtype=np.uint8).reshape(384, 384, 3)).resize((cell, cell))
            os.remove(p)
            d = ImageDraw.Draw(im)
            d.rectangle([0, 0, cell, 14], fill=(20, 20, 20)); d.text((4, 1), f"{name} f{f} | {vn}", fill=(255, 255, 255))
            grid.paste(im, (c * cell, r * cell))
    out = opt.get("out") or os.path.join(HERE, "sheets", f"grid_{name}.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    grid.save(out)
    print(out)


if __name__ == "__main__":
    main()
