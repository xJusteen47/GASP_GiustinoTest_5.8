import sys, json, os
sys.path.insert(0, os.getcwd())
import numpy as np
from animpipe import author as A
for path, k in [a.split('@') for a in sys.argv[1:]]:
    au = json.load(open(path))["author"]; po = A.Poser(au.get("thumb","along"))
    P,_ = po.solve(au["poses"][k])
    up = P["neck_01"]-P["spine_03"]; up/=np.linalg.norm(up)
    side = P["upperarm_l"]-P["upperarm_r"]; side-= (side@up)*up; side/=np.linalg.norm(side)
    fwd = np.cross(up, side)   # check sign with the head offset
    def c(b): v=P[b]-P["spine_05"]; return np.round([v@side, v@fwd, v@up],1)
    if (P["ball_l"]-P["pelvis"])@fwd < -50: pass
    print(os.path.basename(path)[17:30], k, "L sh", c("upperarm_l"), "elb", c("lowerarm_l"), "hand", c("hand_l"), "| R elb", c("lowerarm_r"), "hand", c("hand_r"), "| head", c("head"))
