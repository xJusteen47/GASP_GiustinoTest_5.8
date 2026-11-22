import json, sys, math
d = json.load(open(sys.argv[1] if len(sys.argv) > 1 else "blockout_dump.json"))
J = d["joints_pos"]
keys = list(next(iter(J.values())).keys())
def p(n, f): return J[n][f]
print("frames", keys)
for n in ["pelvis", "spine_05", "head", "upperarm_r", "lowerarm_r", "hand_r", "upperarm_l", "lowerarm_l", "hand_l", "foot_r", "ball_r", "foot_l", "ball_l", "calf_r", "calf_l"]:
    print(f"{n:12s}", "  ".join(f"{f}:{p(n,f)}" for f in keys))
print()
for f in keys:
    sh = p("upperarm_r", f); h = p("hand_r", f); e = p("lowerarm_r", f)
    shl = p("upperarm_l", f); hl = p("hand_l", f); el = p("lowerarm_l", f)
    ext_r = math.dist(sh, h) / (math.dist(sh, e) + math.dist(e, h))
    ext_l = math.dist(shl, hl) / (math.dist(shl, el) + math.dist(el, hl))
    stance = math.dist(p("foot_r", f)[::2], p("foot_l", f)[::2])
    print(f"f{f}: arm extension r={ext_r:.2f} l={ext_l:.2f}  hand_r z={h[2]} y={h[1]}  hand_l z={hl[2]} y={hl[1]}  "
          f"pelvis y={p('pelvis', f)[1]}  stance(xz)={stance:.1f}  head z={p('head', f)[2]}  feet z r/l={p('foot_r',f)[2]}/{p('foot_l',f)[2]}")
