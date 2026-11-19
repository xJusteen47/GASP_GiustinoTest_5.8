"""Give a run stop take its root motion: root track = forward Y from run_stop_<side>_root.json (ends at 0, like the GASP stop clips),
pelvis local translation = the authored component position (the authored pose is relative to the root, as the game shows it with
root lock). Edits sheets/raw/<Asset>_tracks.json in place. python run_stop_root.py <side> <Asset>"""
import json, os, sys
side, asset = sys.argv[1], sys.argv[2]
HERE = os.path.dirname(os.path.abspath(__file__)); TOOLS = os.path.abspath(os.path.join(HERE, "..", ".."))
tp = os.path.join(TOOLS, "sheets", "raw", f"{asset}_tracks.json")
T = json.load(open(tp)); R = json.load(open(os.path.join(HERE, f"run_stop_{side}_root.json")))
root0 = T["bones"]["root"][0]
assert abs(root0[3]) + abs(root0[4]) + abs(root0[5]) < 1e-4, "template root is rotated"
for f, k in enumerate(T["bones"]["pelvis"]):
    k[0] += root0[0]; k[1] += root0[1]; k[2] += root0[2]
T["bones"]["root"] = [[0.0, y, 0.0, 0.0, 0.0, 0.0, 1.0] for y in R["root_y"]]
assert len(T["bones"]["root"]) == T["keys"]
json.dump(T, open(tp, "w"))
print(f"{asset}: root {R['root_y'][0]:.1f} -> {R['root_y'][-1]:.1f} cm over {T['keys']} keys")
