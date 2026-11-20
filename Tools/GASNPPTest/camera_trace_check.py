"""Summarise Saved/Logs/gstn_camera_trace.csv (gstn.Camera.Trace 1) in 5 lines: where the camera jitter comes from.

Usage: python Tools/GASNPPTest/camera_trace_check.py [path]. A "kick" is a frame whose height moves against both neighbours by > 2 cm
(up-down-up or down-up-down), i.e. a zig-zag, not a smooth move. Columns compared: in_z = GASP rig output, out_z = final view,
cam_comp_z = the component the anchor moves, stand_z = the anchor's input, root_z = actor location (sim rate),
mesh_stand_z = the first version's mesh-based height. Capsule line (traces with half_height): for each capsule size change (crouch,
aerial, landing, knockdown), how far each stage's height moved in the next 0.5 s; the camera stages (cam_comp, in, out) must stay ~0 on
the ground (owner 2026-11-20: no capsule change may move the camera).
"""
import csv, sys, statistics

path = sys.argv[1] if len(sys.argv) > 1 else "Saved/Logs/gstn_camera_trace.csv"
rows = list(csv.DictReader(open(path)))
def col(k): return [float(r[k]) for r in rows]
def kicks(v, tol=2.0):
    return sum(1 for i in range(1, len(v) - 1) if (v[i] - v[i-1]) * (v[i+1] - v[i]) < 0 and min(abs(v[i] - v[i-1]), abs(v[i+1] - v[i])) > tol)
def flips(k): v = [r[k] for r in rows]; return sum(1 for a, b in zip(v, v[1:]) if a != b)
dt = col("dt")
print(f"frames {len(rows)}  dt median {statistics.median(dt)*1000:.1f} ms  max {max(dt)*1000:.1f} ms  (fps {1/statistics.median(dt):.0f})")
print("zig-zag kicks >2cm: " + "  ".join(f"{k} {kicks(col(k))}" for k in ("root_z", "stand_z", "damped_z", "cam_comp_z", "in_z", "out_z")))
import math
def boom(k): return [math.dist((float(r[k+"_x"]), float(r[k+"_y"]), float(r[k+"_z"])), (float(r["stand_x"]), float(r["stand_y"]), float(r["stand_z"]))) for r in rows]
# a pitched-down camera moving back and forth along its boom also reads as a vertical bounce on screen (parallax): camera-to-body distance
print("boom (camera to drawn body) zig-zags >2cm: " + "  ".join(f"{k} {kicks(boom(k))}" for k in ("in", "out")) + f"  yaw zig-zags >0.3deg: in {kicks(col('in_yaw'), 0.3)}")
print(f"pitch zig-zags >0.3deg: in {kicks(col('in_pitch'), 0.3)}  out {kicks(col('out_pitch'), 0.3)}")
print(f"flips: sweep_hit {flips('sweep_hit')}  anchor_on {flips('anchor_on')}  in_air {flips('in_air')}")
print("first stage with kicks (z or boom) = the culprit (root/stand = movement, cam_comp = anchor, in = GASP rig, out only = our push back/sweep)")
if rows and "half_height" in rows[0]:
    hh, t = col("half_height"), col("time")
    changes = [i for i in range(1, len(rows)) if abs(hh[i] - hh[i-1]) > 0.5 and rows[i]["in_air"] == "0"]
    def moved(k):
        v = col(k); worst = 0.0
        for i in changes:
            j = i
            while j < len(rows) and t[j] - t[i] < 0.5:
                worst = max(worst, abs(v[j] - v[i-1])); j += 1
        return worst
    print(f"grounded capsule changes {len(changes)} (crouch flips {flips('crouch')}): max height move in 0.5 s, cm: "
          + "  ".join(f"{k} {moved(k):.1f}" for k in ("root_z", "mesh_stand_z", "stand_z", "cam_comp_z", "in_z", "out_z")))
