"""Side-by-side of a reference's key poses and our takes, the real mesh from the same side camera (owner's review rule for
reference moves: "a side-by-side of our frames against the owner's frames from the same side camera").

    python ref_sidebyside.py <ref dir> <out.png> <take>:<f,f,f,f,f> [<take>:<frames> ...] [--az=270] [--flip]

One column per reference still (reference.json order), one row for the reference and one per take. Each take's frames are
UE lit captures (view_sheets.lit) after the pose has settled; --flip mirrors our captures so the attack runs the same way
as the reference's."""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import view_sheets as V
import legibility_shots as S
L = S.L
CELL = 360


def fit(im, w=CELL, h=CELL):
    im = im.copy(); im.thumbnail((w, h - 30))
    out = Image.new("RGB", (w, h), (40, 40, 40)); out.paste(im, ((w - im.width) // 2, 30 + (h - 30 - im.height) // 2))
    return out


def main():
    ref_dir = os.path.join(HERE, sys.argv[1]); out = os.path.join(HERE, sys.argv[2])
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[3:] if a.startswith("--"))
    rows = [a for a in sys.argv[3:] if not a.startswith("--")]
    az = float(opt.get("az") or 270)
    R = json.load(open(os.path.join(ref_dir, "reference.json")))
    stills = R["stills"]
    import ue
    ue.py("setup")
    grid = Image.new("RGB", (CELL * len(stills), CELL * (1 + len(rows))), (20, 20, 20))
    for i, s in enumerate(stills):
        im = fit(Image.open(os.path.join(ref_dir, s["image"])).convert("RGB"))
        grid.paste(V.label(im, f"reference: {s['phase']}"), (i * CELL, 0))
    for r, spec in enumerate(rows, start=1):
        take, frames = spec.split(":"); frames = [int(x) for x in frames.split(",")]
        d = json.load(open(os.path.join(L.RAW, f"{take}_legbones.json")))
        pel = np.mean([L.bone_tf(d["frames"][str(f)], "pelvis")[0] for f in frames], axis=0)
        for i, f in enumerate(frames):
            S.settle(d["anim"], f, d["frames"][str(f)])
            im = V.lit(take, f"sbs_{take}_{f}", az, 5.0, dist=330.0, center=[pel[0], pel[1], 95.0])
            if "flip" in opt:
                im = ImageOps.mirror(im)
            grid.paste(V.label(fit(im), f"{take} f{f}"), (i * CELL, r * CELL))
    grid.save(out)
    print("written", out)


if __name__ == "__main__":
    main()
