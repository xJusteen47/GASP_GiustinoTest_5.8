"""Editor-side, read-only: component-space positions of the lead arm (or the left leg, leg=True) sampled between baked frames
(sub=4: quarter frames), to see how Unreal interpolates a clip under time dilation (31/10: the snap arcs around its joint)."""
import unreal, json
CHAIN = ["root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"]

LEG = ["root", "pelvis", "thigh_l", "calf_l", "foot_l", "ball_l"]

def comp(anim, t, upto="hand_l", chain=None):
    """Component-space transform of a bone at time t (seconds), composed from the local poses UE evaluates at t."""
    x = None
    out = {}
    for b in (chain or CHAIN):
        loc = unreal.AnimationLibrary.get_bone_pose_for_time(anim, b, t, False)
        x = loc if x is None else loc.multiply(x)
        out[b] = x
        if b == upto:
            break
    return out

def main(asset="/Game/AnimPipeline/Pilot/AS_Pilot_JabV11", f0=0, f1=4, sub=4, fps=60.0, leg=False):
    anim = unreal.load_asset(asset)
    rows = []
    for i in range(int(f0 * sub), int(f1 * sub) + 1):
        t = i / (sub * fps)
        c = comp(anim, t, "ball_l", LEG) if leg else comp(anim, t)
        h = (c["foot_l"] if leg else c["hand_l"]).translation; e = (c["calf_l"] if leg else c["lowerarm_l"]).translation
        rows.append({"f": round(i / sub, 3), "hand": [round(h.x, 2), round(h.y, 2), round(h.z, 2)], "elbow": [round(e.x, 2), round(e.y, 2), round(e.z, 2)]})
    return json.dumps({"asset": asset, "rows": rows})
