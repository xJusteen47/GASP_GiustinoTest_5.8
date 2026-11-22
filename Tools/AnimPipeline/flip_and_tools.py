import csc, json, math
mv = scene.model_viewer(); dv = scene.data_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
def did(n, dn):
    for d in dv.get_all_data_id(ids[n]):
        try:
            if dv.get_data_name(d) == dn: return d
        except Exception: pass
def q(r):
    x = r.to_quaternion(); g = lambda v: float(v() if callable(v) else v)
    return (g(x.w), g(x.x), g(x.y), g(x.z))
def ang(a, b):
    return math.degrees(2 * math.acos(min(1.0, abs(sum(i * j for i, j in zip(a, b))))))
out = {}
for j in ["upperarm_r", "hand_r", "lowerarm_twist_01_r", "lowerarm_r"]:
    d = did(j, "Local Rotation")
    qs = [q(dv.get_data_value(d, f)) for f in range(8, 18)]
    out[j] = {8 + i: round(ang(qs[i], qs[i + 1]), 1) for i in range(len(qs) - 1)}
tm = csc.app.get_application().get_tools_manager()
try:
    out["tools"] = sorted(str(t) for t in tm.tools())
except Exception as e:
    out["tools_err"] = str(e)

print(json.dumps(out, default=str))
