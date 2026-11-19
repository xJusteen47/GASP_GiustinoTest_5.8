"""Stick-figure grid of a run stop take (side view from the character's right, and front view), for a quick pose read before Unreal.
python run_stop_plot.py <bones.json> <out.png> [frames=2,6,...]   Prints one line."""
import json, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

src, out = sys.argv[1], sys.argv[2]
FR = [int(x) for x in sys.argv[3].split(",")] if len(sys.argv) > 3 else [2, 6, 10, 14, 20, 26, 30, 34, 38, 42, 46, 52, 60]
F = json.load(open(src))["frames"]
CH = [["pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "neck_02", "head"]]
for s, c in (("l", "tab:blue"), ("r", "tab:red")):
    CH += [["spine_05", f"clavicle_{s}", f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"], ["pelvis", f"thigh_{s}", f"calf_{s}", f"foot_{s}", f"ball_{s}"]]
COL = ["k", "tab:blue", "tab:blue", "tab:red", "tab:red"]
fig, ax = plt.subplots(2, len(FR), figsize=(1.6 * len(FR), 6.4))
for i, f in enumerate(FR):
    P = {b: v[0] for b, v in F[str(f)].items()}
    for row, (h, lab) in enumerate(((1, "Y fwd"), (0, "X left"))):
        a = ax[row][i]
        for ch, c in zip(CH, COL):
            pts = [P[b] for b in ch if b in P]
            a.plot([p[h] if row == 0 else -p[h] for p in pts], [p[2] for p in pts], "-o", color=c, ms=1.5, lw=1.5)
        a.axhline(0, color="0.6", lw=0.8)
        a.set_xlim(-70, 70); a.set_ylim(-5, 175); a.set_aspect("equal"); a.set_xticks([]); a.set_yticks([])
        if row == 0:
            a.set_title(f"f{f}", fontsize=8)
fig.tight_layout()
fig.savefig(out, dpi=110)
print(out)
