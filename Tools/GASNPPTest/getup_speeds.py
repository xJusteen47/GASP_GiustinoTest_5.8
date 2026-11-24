"""getup-momentum-2: capsule XY speed (cm/s) and travel heading (deg) per get-up capture, FAF..+6 and +6..+20 after the get-up start.
    python Tools/GASNPPTest/getup_speeds.py <capture_dir> <faf_frame> 60   (captures are 60 fps; same numbers as getup-momentum-1 speeds.txt)"""
import json, sys, math, glob, os
root, faf = sys.argv[1], int(sys.argv[2])
fps = float(sys.argv[3]) if len(sys.argv) > 3 else 30.0
for p in sorted(glob.glob(os.path.join(root, '*', 'frames.jsonl'))):
    L = [json.loads(l) for l in open(p)]
    g = [d for d in L if d['seg'] == 'getup']
    if not g:
        print(os.path.basename(os.path.dirname(p)), 'no getup seg'); continue
    g0 = g[0]['f']
    byf = {d['f']: d for d in L}
    def sp(a, b):
        A, B = byf.get(g0 + a), byf.get(g0 + b)
        if not A or not B: return None
        dx, dy = B['loc'][0] - A['loc'][0], B['loc'][1] - A['loc'][1]
        return round(math.hypot(dx, dy) / ((b - a) / fps)), round(math.degrees(math.atan2(dy, dx)))
    f0 = byf[g0 + faf]; m = byf.get(g0 + faf + 3, {}).get('montage')
    print(os.path.basename(os.path.dirname(p)), 'FAF', faf, 'FAF..+6', sp(faf, faf + 6), '+6..+20', sp(faf + 6, faf + 20),
          'yaw@0', g[0]['yaw'], 'yaw@FAF', f0['yaw'], 'mont@FAF+3', m)
