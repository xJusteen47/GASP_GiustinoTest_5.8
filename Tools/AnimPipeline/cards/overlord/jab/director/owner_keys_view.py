"""Draw the owner's Cascadeur key poses (owner_keys_*.json) as thick stick figures: front, side, top rows."""
import json, sys
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
src = sys.argv[1]; out = sys.argv[2]
doc = json.load(open(src)); F = doc["frames"]
frames = [f for f in doc["meta"]["key_frames"] if f != 140]
CH = [("pelvis","spine_02","spine_04","neck_01","head"),
      ("clavicle_r","upperarm_r","lowerarm_r","hand_r","middle_03_r"),("clavicle_l","upperarm_l","lowerarm_l","hand_l","middle_03_l"),
      ("pelvis","thigh_r","calf_r","foot_r","ball_r"),("pelvis","thigh_l","calf_l","foot_l","ball_l")]
COL = ["k","tab:red","tab:blue","tab:orange","tab:cyan"]
# views: (name, horizontal axis idx, sign, vertical idx, sign)
V = [("front (char right = image left)",0,-1,1,1),("side (forward = right)",2,1,1,1),("top (forward = up, right = right)",0,-1,2,1)]
fig, ax = plt.subplots(3, len(frames), figsize=(3.2*len(frames), 10))
for c, f in enumerate(frames):
    P = {k: v["global_position"] for k, v in F[str(f)].items() if "global_position" in v}
    for r, (nm, hi, hs, vi, vs) in enumerate(V):
        a = ax[r][c]
        for ch, col in zip(CH, COL):
            pts = [P[j] for j in ch if j in P]
            a.plot([p[hi]*hs for p in pts], [p[vi]*vs for p in pts], "-o", color=col, lw=5, ms=4, solid_capstyle="round")
        h = P["head"]; a.add_patch(plt.Circle((h[hi]*hs, h[vi]*vs + (8 if vi==1 else 0)), 10, color="k", fill=False, lw=2))
        a.set_aspect("equal"); a.set_xlim(-110,110); a.set_ylim((-5,175) if vi==1 else (-70,110))
        a.axhline(0, color="gray", lw=.5); a.axvline(0, color="gray", lw=.5, ls=":")
        a.set_title(f"f{f}  {nm}" if c==0 else f"f{f}", fontsize=9); a.tick_params(labelsize=6)
plt.tight_layout(); plt.savefig(out, dpi=70)
print("ok", out)
