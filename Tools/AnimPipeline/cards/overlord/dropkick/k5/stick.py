import sys, json, os
sys.path.insert(0, os.getcwd())
import numpy as np
from PIL import Image, ImageDraw
from animpipe import author as A
card = json.load(open(sys.argv[1])); out = sys.argv[2]
au = card["author"]; po = A.Poser(au.get("thumb", "along"))
keys = sorted(au["poses"], key=int)
if len(sys.argv) > 3: keys = sys.argv[3].split(",")
CH = [("pelvis","spine_03"),("spine_03","spine_05"),("spine_05","neck_01"),("neck_01","head")]
for s in "lr":
    CH += [(f"spine_05",f"clavicle_{s}"),(f"clavicle_{s}",f"upperarm_{s}"),(f"upperarm_{s}",f"lowerarm_{s}"),(f"lowerarm_{s}",f"hand_{s}"),
           ("pelvis",f"thigh_{s}"),(f"thigh_{s}",f"calf_{s}"),(f"calf_{s}",f"foot_{s}"),(f"foot_{s}",f"ball_{s}")]
COL = {"lowerarm_l":"blue","hand_l":"blue","upperarm_l":"blue","lowerarm_r":"red","hand_r":"red","upperarm_r":"red",
       "calf_l":"teal","foot_l":"teal","ball_l":"teal","calf_r":"orange","foot_r":"orange","ball_r":"orange"}
C = 220; G = Image.new("RGB", (C*len(keys), C*2), "white"); d = ImageDraw.Draw(G)
for i, k in enumerate(keys):
    P, R = po.solve(au["poses"][k])
    c = P["spine_03"]
    for row, (ax, sx) in enumerate([((1, 2), 1), ((0, 2), -1)]):   # side: Y right; front: X (left) shown right->left
        ox, oy = i*C + C//2, row*C + C//2
        def pt(b):
            v = P[b] - c
            return (ox + sx*v[ax[0]]*1.0, oy - v[ax[1]]*1.0)
        for a, b in CH:
            d.line([pt(a), pt(b)], fill=COL.get(b, "black"), width=3)
        d.ellipse([pt("head")[0]-9, pt("head")[1]-9, pt("head")[0]+9, pt("head")[1]+9], outline="black", width=2)
    d.text((i*C+4, 2), f"f{k}", fill="black")
    # crossed-arm numbers in the chest frame: hand offset from spine_05, forearm direction vs chest normal
    if "hand_l" in P:
        s5 = P["spine_05"]; up = P["neck_01"] - P["spine_03"]; up /= np.linalg.norm(up)
        fa = P["hand_l"] - P["lowerarm_l"]; fa /= np.linalg.norm(fa)
        print(k, "hand_l-s5", np.round(P["hand_l"]-s5,1), "hand_r-s5", np.round(P["hand_r"]-s5,1),
              "elbowL-s5", np.round(P["lowerarm_l"]-s5,1), "forearmL.up %.2f" % float(fa@up),
              "ball_l", np.round(P["ball_l"]-P["pelvis"],0))
G.save(out); print(out)
