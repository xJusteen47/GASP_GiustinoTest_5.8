"""Close-ups of the deforming joints (head/neck, shoulders, wrists) at the key frames, 3 angles each: MANDATORY on every review
page, and looked at by the reviewer before delivery (coordinator, 01/11). Ganondorf v3 shipped with the head 17 cm off the
neck: every check ran on whole-body views where the head still read as a head; at 90 cm it is obvious.

    python joint_closeups.py <take> "<frame:label,...>" [--joints=neck,shoulder_l,shoulder_r,wrist_l,wrist_r]

Needs Unreal (the lit capture of capture_ue.py in L_PilotCapture) and sheets/raw/<take>_legbones.json (legibility.sample).
Output: sheets/closeups_<take>/<take>_<joint>.png, one sheet per joint (rows = key frames, columns = angles); each tile is
labelled with that joint's bone-length deviation (bone_lengths.py, cm) so a stretch is named, not only seen."""
import sys, os, json
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import legibility as L
import bone_lengths as BL
from legibility_shots import settle, font

TILE = 384
DIST, FOV, EL = 90.0, 30.0, 15.0
# joint: (centre between these bones, bones whose length is reported, camera azimuths: 3/4 front, side, 3/4 rear)
JOINTS = {"neck": (("neck_01", "head"), ("neck_01", "neck_02", "head"), (45, 90, 225)),
          "shoulder_l": (("clavicle_l", "upperarm_l"), ("clavicle_l", "upperarm_l"), (45, 90, 135)),
          "shoulder_r": (("clavicle_r", "upperarm_r"), ("clavicle_r", "upperarm_r"), (315, 270, 225)),
          "wrist_l": (("lowerarm_l", "hand_l"), ("hand_l", "lowerarm_twist_02_l"), (45, 90, 135)),
          "wrist_r": (("lowerarm_r", "hand_r"), ("hand_r", "lowerarm_twist_02_r"), (315, 270, 225))}


def main():
    take = sys.argv[1]; L.configure(take)
    poses = [(int(a), b) for a, b in (x.split(":") for x in sys.argv[2].split(","))]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[3:] if a.startswith("--"))
    joints = (opt.get("joints") or ",".join(JOINTS)).split(",")
    src = os.path.join(L.RAW, f"{take}_legbones.json")
    d = json.load(open(src))
    lens = BL.lengths(src)
    import ue
    ue.py("setup")
    out_dir = os.path.join(L.SHEETS, f"closeups_{take}")
    os.makedirs(out_dir, exist_ok=True)
    tiles = {j: {} for j in joints}
    for frame, label in poses:
        fr = d["frames"][str(frame)]
        settle(d["anim"], frame, fr)
        for j in joints:
            pair, report, azs = JOINTS[j]
            c = 0.5 * (L.bone_tf(fr, pair[0])[0] + L.bone_tf(fr, pair[1])[0])
            for az in azs:
                tag = f"close_{take}_{j}_f{frame}_{az}"
                ue.py("capture", kind="lit", az=az, el=EL, tag=tag, size=TILE, dist=DIST, fov=FOV, center=[float(v) for v in c])
                tiles[j][(frame, az)] = np.fromfile(os.path.join(L.RAW, tag + ".rgb"), dtype=np.uint8).reshape(TILE, TILE, 3)
    for j in joints:
        pair, report, azs = JOINTS[j]
        TOP, LH = 40, 44
        W, H = 150 + TILE * len(azs), TOP + (TILE + LH) * len(poses)
        sheet = Image.new("RGB", (W, H), (24, 24, 24)); dr = ImageDraw.Draw(sheet)
        dr.text((10, 8), f"{take}  {j} close-up  ({DIST:.0f} cm, FOV {FOV:.0f}; columns az {', '.join(map(str, azs))})", fill=(255, 255, 255), font=font(22))
        for r, (frame, label) in enumerate(poses):
            y = TOP + r * (TILE + LH)
            dr.text((10, y + TILE // 2), f"f{frame}\n{label}", fill=(230, 230, 230), font=font(20))
            for k, az in enumerate(azs):
                sheet.paste(Image.fromarray(tiles[j][(frame, az)]), (150 + k * TILE, y))
            dev = []
            for b in report:
                if b in lens:
                    dv = lens[b][frame] - lens[b][0]
                    dev.append(f"{b} {lens[b][frame]:.2f} cm ({dv:+.2f} vs f0)")
            bad = any(abs(lens[b][frame] - lens[b][0]) > BL.TOL for b in report if b in lens)
            dr.text((150, y + TILE + 8), "   ".join(dev), fill=(255, 90, 90) if bad else (200, 200, 200), font=font(17))
        p = os.path.join(out_dir, f"{take}_{j}.png"); sheet.save(p); print(p)


if __name__ == "__main__":
    main()
