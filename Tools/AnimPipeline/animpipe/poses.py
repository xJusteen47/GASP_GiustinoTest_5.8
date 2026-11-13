"""animpipe poses <card> [--frames=0,2,5] [--take=<take>] [--over=<take>]: the pose checkpoint (brief §2.6), one composite
image per move: the owner's reference frame on top (when the pose's source is an image), then side and front stick
figures of each pose (left limbs red, right blue). --over draws a second take in grey under the first (A/B, v(n) vs
v(n+1)). Reads the bone caches (sheets/raw/<take>_legbones.json, component space X left, Y forward, Z up)."""
import json, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.image as mpimg
from animpipe.card import load, REFS
from animpipe.__main__ import TOOLS

CHAINS = [("pelvis", "spine_01", "spine_03", "spine_05", "neck_01", "head"),
          ("spine_05", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "middle_01_l"),
          ("spine_05", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "middle_01_r"),
          ("pelvis", "thigh_l", "calf_l", "foot_l", "ball_l"), ("pelvis", "thigh_r", "calf_r", "foot_r", "ball_r")]
COL = ["k", "#d62728", "#1f77b4", "#d62728", "#1f77b4"]


def _frames(take):
    return json.load(open(os.path.join(TOOLS, "sheets", "raw", f"{take}_legbones.json")))["frames"]


def _draw(a, fr, i, j, grey=False):
    for ch, col in zip(CHAINS, COL):
        a.plot([fr[b][i] for b in ch], [fr[b][j] for b in ch], color="0.7" if grey else col, lw=1.5 if grey else 2.2,
               marker="o", ms=2)
    if not grey:
        a.plot(fr["head"][i], fr["head"][j], "ko", ms=9, mfc="none")


def run(path, opt):
    c = load(path)
    take = opt.get("take") or c["source_anim"].rsplit("/", 1)[1][3:]
    F = _frames(take)
    O = _frames(opt["over"]) if opt.get("over") else None
    poses = {p["frame"]: p for p in c["poses"]}
    if opt.get("frames"):
        sel = [int(x) for x in opt["frames"].split(",")]
    else:
        sel = [f for f, p in poses.items() if p.get("source") and str(p["source"].get("file", "")).endswith(".png")][:5]
        if not sel:                              # --take/--over without reference stills: the card's own key poses, <= 6 spread out
            ks = sorted(poses)
            sel = [ks[round(i * (len(ks) - 1) / 5)] for i in range(6)] if len(ks) > 6 else ks
            sel = sorted(set(sel))
    imgs = [poses.get(f, {}).get("source") for f in sel]
    has_img = any(s and str(s.get("file", "")).endswith(".png") for s in imgs)
    rows = 3 if has_img else 2
    fig, ax = plt.subplots(rows, len(sel), figsize=(3.4 * len(sel), 3.6 * rows), squeeze=False)
    for k, f in enumerate(sel):
        r = 0
        if has_img:
            s = imgs[k]
            if s and str(s.get("file", "")).endswith(".png"):
                ax[0][k].imshow(mpimg.imread(os.path.join(REFS, s["file"])))
            ax[0][k].axis("off"); r = 1
        name = poses.get(f, {}).get("name", "")
        for rr, (i, j, lab) in enumerate(((1, 2, "side"), (0, 2, "front"))):
            a = ax[r + rr][k]
            if O is not None:
                _draw(a, O[str(f)], i, j, grey=True)
            _draw(a, F[str(f)], i, j)
            a.set_aspect("equal"); a.set_xlim(-90, 90) if i == 0 else a.set_xlim(-80, 100); a.set_ylim(0, 190)
            a.set_xticks([]); a.set_yticks([])
            a.set_title(f"{f} {name} ({lab})", fontsize=9)
    if opt.get("blind"):
        fig.suptitle(f"version {opt['blind']}  (left limbs red, right blue)")
    else:
      fig.suptitle(f"{c['move']} key poses: {take}" + (f" over {opt['over']} (grey)" if O else "") + "  (left limbs red, right blue)")
    fig.tight_layout()
    out = os.path.join(TOOLS, "sheets", f"{c['move']}_poses{'_' + opt['tag'] if opt.get('tag') else ''}.png")
    fig.savefig(out, dpi=70)
    print(f"poses {c['move']}: frames {sel} from {take}" + (f" over {opt['over']}" if O else ""))
    print(out)
