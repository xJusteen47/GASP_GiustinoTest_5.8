"""Measure Cascadeur -> Unreal Live Link latency: apply ll_edit.py (DY cm on the lead hand at FRAME), then poll the
Live Link AnimSequence in Unreal until hand_l at FRAME moves. Usage: python ll_latency.py <DY> [frame]"""
import sys, os, json, time, subprocess, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue
HERE = os.path.dirname(os.path.abspath(__file__))
ANIM = "/Game/AS_Pilot_JabLiveLink"
DY = float(sys.argv[1]); F = int(sys.argv[2]) if len(sys.argv) > 2 else 10
def hand():
    r = ue.call("sample_pose", {"assetPath": ANIM, "boneNames": ["hand_l"], "frames": [F], "space": "component"})
    L = r["samples"][0]["bones"][0]["location"]; return (L["x"], L["y"], L["z"])
h0 = hand()
t_req = time.time()
out = subprocess.run([sys.executable, os.path.join(HERE, "csc_run.py"), os.path.join(HERE, "ll_edit.py"), f"FRAME={F}", f"DY={DY}"],
                     capture_output=True, text=True)
line = [l for l in out.stdout.splitlines() if "t_done" in l][0]
t_done = json.loads(line[line.index("{"):])["t_done"]
polls = 0
while time.time() - t_done < 30:
    h = hand(); polls += 1
    if math.dist(h, h0) > 0.5 * abs(DY):
        t_seen = time.time(); break
    time.sleep(0.1)
else:
    t_seen = None
print(json.dumps({"dy": DY, "frame": F, "script_roundtrip_s": round(t_done - t_req, 2),
                  "latency_after_edit_s": None if t_seen is None else round(t_seen - t_done, 2),
                  "hand_before": [round(x, 2) for x in h0], "hand_after": [round(x, 2) for x in hand()], "polls": polls}))
