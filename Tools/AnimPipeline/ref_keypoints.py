"""2D keypoints of a reference (stills or a GIF) with rtmlib (RTMPose, HumanArt-trained detector: handles game characters).

Runs in the isolated venv (numpy, onnxruntime, opencv; no mmcv):
    C:/Users/jacop/Tools/animcurves-venv/Scripts/python ref_keypoints.py <out.json> <image|gif> [<image> ...] [--frames=a-b] [--mode=balanced]
Writes, per image (or GIF frame): COCO-17 keypoints in pixels (y down) and scores, plus an overlay PNG next to <out.json>
(<out>_<i>.png) to check the labels by eye. The first run downloads the ONNX models from download.openmmlab.com
(Hugging Face mirror on failure) into ~/.cache/rtmlib.

Side views often swap left/right labels. Fix them in a sibling `<out>_fix.json` ({"<image name>": {"swap": ["arms", "legs"],
"set": {"wrist_l": [x, y]}}}), which motion_ref.py applies; when the estimator fails on a frame (cape, hitbox overlay),
mark that frame by hand there ("set" every point). The manual file wins over the estimator.
"""
import json
import os
import sys

import numpy as np

COCO = ["nose", "eye_l", "eye_r", "ear_l", "ear_r", "shoulder_l", "shoulder_r", "elbow_l", "elbow_r", "wrist_l", "wrist_r",
        "hip_l", "hip_r", "knee_l", "knee_r", "ankle_l", "ankle_r"]
BONES = [(5, 7), (7, 9), (6, 8), (8, 10), (11, 13), (13, 15), (12, 14), (14, 16), (5, 6), (11, 12), (5, 11), (6, 12), (0, 5), (0, 6)]


def load_images(paths, frames=None):
    from PIL import Image
    out = []
    for p in paths:
        im = Image.open(p)
        n = getattr(im, "n_frames", 1)
        if n > 1:
            idx = range(n) if frames is None else range(frames[0], min(frames[1], n - 1) + 1)
            for i in idx:
                im.seek(i)
                out.append((f"{os.path.basename(p)}#{i}", np.array(im.convert("RGB"))[:, :, ::-1].copy()))
        else:
            out.append((os.path.basename(p), np.array(im.convert("RGB"))[:, :, ::-1].copy()))
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opts = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--") and "=" in a)
    out, paths = args[0], args[1:]
    frames = tuple(int(x) for x in opts["frames"].split("-")) if "frames" in opts else None
    import cv2
    from rtmlib import Body
    body = Body(mode=opts.get("mode", "balanced"), backend="onnxruntime", device="cpu")
    res = {"_doc": "COCO-17 keypoints in image pixels (x right, y down), score 0-1; from ref_keypoints.py (rtmlib Body)",
           "names": COCO, "images": {}}
    stem = os.path.splitext(out)[0]
    for i, (name, img) in enumerate(load_images(paths, frames)):
        kps, sc = body(img)
        if len(kps) == 0:
            res["images"][name] = None
            print(f"{name}: no person")
            continue
        k = int(np.argmax([s.mean() for s in sc]))                   # the best-scored person (there is only one)
        kp, s = kps[k], sc[k]
        res["images"][name] = {"size": [img.shape[1], img.shape[0]],
                               "points": {n: [round(float(kp[j][0]), 1), round(float(kp[j][1]), 1)] for j, n in enumerate(COCO)},
                               "score": {n: round(float(s[j]), 3) for j, n in enumerate(COCO)}}
        vis = img.copy()
        for a, b in BONES:
            col = (0, 200, 255) if "_l" in COCO[a] or "_l" in COCO[b] else (255, 120, 0)
            cv2.line(vis, tuple(int(v) for v in kp[a]), tuple(int(v) for v in kp[b]), col, 3)
        for j, n in enumerate(COCO):
            cv2.circle(vis, tuple(int(v) for v in kp[j]), 6, (0, 255, 0) if s[j] > 0.3 else (0, 0, 255), -1)
            cv2.putText(vis, n, (int(kp[j][0]) + 6, int(kp[j][1]) - 6), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
        cv2.imwrite(f"{stem}_{i}.png", vis)
        print(f"{name}: mean score {s.mean():.2f}, low: {[n for j, n in enumerate(COCO) if s[j] < 0.3]}")
    json.dump(res, open(out, "w"), indent=1)


if __name__ == "__main__":
    main()
