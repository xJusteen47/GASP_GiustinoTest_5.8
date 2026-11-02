"""Visual check driver: silhouette contact sheet, lit strip, video, silhouette metrics.
Usage: python visual_check.py <AnimPath> <take_name>"""
import sys, os, time, json, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image, ImageDraw
import ue

ANIM, TAKE = sys.argv[1], sys.argv[2]
KEYS = [int(x) for x in (sys.argv[3] if len(sys.argv) > 3 else "0,1,3,8,17,37").split(",")]
NFRAMES = int(sys.argv[4]) if len(sys.argv) > 4 else 38
LIT_AZ = int(sys.argv[5]) if len(sys.argv) > 5 else 315
ARM_BONE = sys.argv[6] if len(sys.argv) > 6 else "clavicle_r"
AZ = [0, 45, 90, 135, 180, 225, 270, 315]   # 0 = front, 90 = character's left side, 270 = its right side
if os.environ.get("VC_AZ"):
    AZ = [int(x) for x in os.environ["VC_AZ"].split(",")]   # e.g. VC_AZ=0,90,270,315 for intermediate passes
EL = 15
SHEETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sheets")
RAW = os.path.join(SHEETS, "raw")
t_start = time.time()

_expected = {}
def _expect(frame):
    if frame not in _expected:
        r = ue.call("sample_pose", {"assetPath": ANIM, "boneNames": ["hand_r", "hand_l"], "frames": [frame], "space": "component"})
        b = {x["name"]: x["location"] for x in r["samples"][0]["bones"]}
        _expected[frame] = (b["hand_r"]["x"], b["hand_r"]["y"], b["hand_r"]["z"])
    return _expected[frame]

def pose(frame, hide=False):
    # The pose only updates on an editor tick, and bone hiding only shows after the pose is re-evaluated:
    # pass through another frame, then repeat until the live hand position matches the sampled asset pose.
    ue.py("set_frame", anim_path=ANIM, frame=(frame + 5) % NFRAMES, hide_arm=hide, bone=ARM_BONE)
    want = _expect(frame)
    for _ in range(30):
        got = ue.py("set_frame", anim_path=ANIM, frame=frame, hide_arm=hide, bone=ARM_BONE)["hand_r"]
        if max(abs(a - b) for a, b in zip(got, want)) < 0.5:
            ue.py("set_frame", anim_path=ANIM, frame=frame, hide_arm=hide, bone=ARM_BONE)  # one more tick for the render state
            return got
        time.sleep(0.2)
    raise RuntimeError(f"pose for frame {frame} did not settle: got {got}, want {want}")

def mask(tag):
    d = np.fromfile(os.path.join(RAW, tag + ".f32"), dtype=np.float32).reshape(256, 256)
    return d < 10000

ue.py("setup")
metrics = {}
sheet = Image.new("L", (256 * len(AZ) + 60, 256 * len(KEYS) + 30), 255)
draw = ImageDraw.Draw(sheet)
for c, az in enumerate(AZ):
    draw.text((60 + c * 256 + 100, 8), f"az {az}", fill=0)
for r, f in enumerate(KEYS):
    draw.text((8, 30 + r * 256 + 120), f"f{f}", fill=0)
    pose(f, False)
    full = {}
    for az in AZ:
        tag = f"{TAKE}_f{f}_az{az}"
        ue.py("capture", kind="depth", az=az, el=EL, tag=tag)
        full[az] = mask(tag)
    pose(f, True)
    torso = {}
    for az in AZ:
        tag = f"{TAKE}_f{f}_az{az}_noarm"
        ue.py("capture", kind="depth", az=az, el=EL, tag=tag)
        torso[az] = mask(tag)
    pose(f, False)
    areas = {az: int(full[az].sum()) for az in AZ}
    best = max(areas.values())
    row = {}
    for c, az in enumerate(AZ):
        arm_out = int((full[az] & ~torso[az]).sum())  # right-arm pixels that are not on top of the body silhouette
        row[az] = {"area_ratio": round(areas[az] / best, 2), "arm_out_px": arm_out,
                   "arm_out_pct_of_sil": round(100 * arm_out / max(1, areas[az]), 1)}
        img = Image.fromarray(np.where(full[az], 0, 255).astype(np.uint8))
        sheet.paste(img, (60 + c * 256, 30 + r * 256))
        draw.rectangle([60 + c * 256, 30 + r * 256, 60 + c * 256 + 255, 30 + r * 256 + 255], outline=180)
    metrics[f] = row
os.makedirs(SHEETS, exist_ok=True)
sheet.save(os.path.join(SHEETS, f"{TAKE}_silhouettes.png"))
t_sil = time.time() - t_start

# lit strip + video frames, 3/4 front view from the character's right (the punching side)
t1 = time.time()
frames_dir = os.path.join(RAW, f"{TAKE}_lit")
os.makedirs(frames_dir, exist_ok=True)
cells = []
for f in range(NFRAMES):
    pose(f, False)
    ue.py("capture", kind="lit", az=LIT_AZ, el=10, tag=f"{TAKE}_lit_{f:02d}")
    rgb = np.fromfile(os.path.join(RAW, f"{TAKE}_lit_{f:02d}.rgb"), dtype=np.uint8).reshape(512, 512, 3)
    im = Image.fromarray(rgb)
    ImageDraw.Draw(im).text((10, 10), f"{TAKE}  f{f}", fill=(255, 255, 255))
    im.save(os.path.join(frames_dir, f"{f:03d}.png"))
    cells.append(im.resize((256, 256)))
cols = 10
strip = Image.new("RGB", (256 * cols, 256 * ((len(cells) + cols - 1) // cols)), (0, 0, 0))
for i, im in enumerate(cells):
    strip.paste(im, ((i % cols) * 256, (i // cols) * 256))
strip.save(os.path.join(SHEETS, f"{TAKE}_strip.png"))
for name, rate in ((f"{TAKE}.mp4", 60), (f"{TAKE}_slow.mp4", 10)):
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(rate), "-i", os.path.join(frames_dir, "%03d.png"),
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", os.path.join(SHEETS, name)], check=True)
t_lit = time.time() - t1
json.dump(metrics, open(os.path.join(SHEETS, f"{TAKE}_metrics.json"), "w"), indent=1)
print(json.dumps({"take": TAKE, "sec_silhouettes": round(t_sil, 1), "sec_lit_and_video": round(t_lit, 1), "metrics": metrics}))



